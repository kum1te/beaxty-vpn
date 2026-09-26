// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QByteArrayView>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <limits>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

namespace {

constexpr qint64 kMaximumCoreSize = 512LL * 1024 * 1024;

bool rootOwnedNotWritableByOthers(const QFileInfo &info) {
    const QFile::Permissions writableByOthers = QFile::WriteGroup | QFile::WriteOther;
    return info.ownerId() == 0 && !(info.permissions() & writableByOthers);
}

bool trustedRootDirectoryChain(const QString &directory) {
    QString current = QFileInfo(directory).canonicalFilePath();
    if (current.isEmpty()) return false;
    while (true) {
        const QFileInfo info(current);
        if (!info.isDir() || !rootOwnedNotWritableByOthers(info)) return false;
        const QString parent = info.dir().absolutePath();
        if (parent == current) return true;
        current = parent;
    }
}

QString trustedSystemExecutable(const QStringList &candidates) {
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        const QString canonical = info.canonicalFilePath();
        if (info.isFile() && info.isExecutable() && rootOwnedNotWritableByOthers(info) &&
            !canonical.isEmpty() && trustedRootDirectoryChain(QFileInfo(canonical).absolutePath())) {
            return canonical;
        }
    }
    return {};
}

bool runTool(const QString &path, const QStringList &arguments, QByteArray *standardOutput = nullptr) {
    if (path.isEmpty()) return false;
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(path, arguments);
    if (!process.waitForStarted(3000) || !process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    if (standardOutput) *standardOutput = process.readAllStandardOutput().trimmed();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool directoryHasAcl(const QString &getfaclPath, const QString &path, const QByteArray &expected) {
    QByteArray output;
    return runTool(getfaclPath, {QStringLiteral("-cpn"), QStringLiteral("--"), path}, &output) &&
           output == expected;
}

bool ensureRootDirectory(const QString &path, mode_t mode) {
    const QByteArray nativePath = QFile::encodeName(path);
    if (::mkdir(nativePath.constData(), mode) != 0 && errno != EEXIST) return false;

    struct stat st {};
    if (::lstat(nativePath.constData(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != 0 ||
        (st.st_mode & (S_IWGRP | S_IWOTH | S_ISUID | S_ISGID))) {
        return false;
    }
    const QFileInfo info(path);
    return info.canonicalFilePath() == path && trustedRootDirectoryChain(path);
}

bool hasCapabilities(int fd) {
    std::array<char, 256> value{};
    const ssize_t size = ::fgetxattr(fd, "security.capability", value.data(), value.size());
    if (size >= 0) return true;
    return errno != ENODATA && errno != ENOTSUP;
}

bool sourceFileIsSafe(int fd, uid_t callerUid, struct stat *sourceStat) {
    if (::fstat(fd, sourceStat) != 0 || !S_ISREG(sourceStat->st_mode) ||
        (sourceStat->st_uid != callerUid && sourceStat->st_uid != 0) ||
        !(sourceStat->st_mode & S_IXUSR) ||
        (sourceStat->st_mode & (S_IWGRP | S_IWOTH | S_ISUID | S_ISGID)) ||
        sourceStat->st_size <= 0 || sourceStat->st_size > kMaximumCoreSize || hasCapabilities(fd)) {
        return false;
    }
    return true;
}

QByteArray hashFileDescriptor(int fd, int outputFd = -1) {
    if (::lseek(fd, 0, SEEK_SET) < 0) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, 256 * 1024> buffer{};
    qint64 total = 0;
    while (true) {
        const ssize_t count = ::read(fd, buffer.data(), buffer.size());
        if (count == 0) break;
        if (count < 0) {
            if (errno == EINTR) continue;
            return {};
        }
        total += count;
        if (total > kMaximumCoreSize) return {};
        hash.addData(QByteArrayView(buffer.data(), count));
        if (outputFd >= 0) {
            ssize_t written = 0;
            while (written < count) {
                const ssize_t result = ::write(outputFd, buffer.data() + written,
                                               static_cast<size_t>(count - written));
                if (result < 0 && errno == EINTR) continue;
                if (result <= 0) return {};
                written += result;
            }
        }
    }
    return hash.result().toHex();
}

bool hashPathMatches(const QString &path, const QByteArray &expectedDigest) {
    const QByteArray nativePath = QFile::encodeName(path);
    const int fd = ::open(nativePath.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return false;
    struct stat st {};
    const bool safe = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == 0 &&
                      (st.st_mode & S_IXUSR) &&
                      !(st.st_mode & (S_IWGRP | S_IWOTH | S_ISUID | S_ISGID));
    const QByteArray actual = safe ? hashFileDescriptor(fd) : QByteArray();
    ::close(fd);
    return !actual.isEmpty() && actual == expectedDigest;
}

bool copyCoreWithoutOverwrite(int sourceFd, const QString &userDirectory,
                               const QString &targetPath, const QByteArray &expectedDigest) {
    const QByteArray nativeDirectory = QFile::encodeName(userDirectory);
    const int directoryFd = ::open(nativeDirectory.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (directoryFd < 0) return false;

    const QByteArray targetName = QFile::encodeName(QFileInfo(targetPath).fileName());
    const QByteArray temporaryName = QByteArrayLiteral(".beaxty-core-") +
        QUuid::createUuid().toString(QUuid::Id128).toLatin1();
    const int outputFd = ::openat(directoryFd, temporaryName.constData(),
                                  O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (outputFd < 0) {
        ::close(directoryFd);
        return false;
    }

    const QByteArray actualDigest = hashFileDescriptor(sourceFd, outputFd);
    bool success = !actualDigest.isEmpty() && actualDigest == expectedDigest &&
                   ::fchown(outputFd, 0, 0) == 0 && ::fchmod(outputFd, 0755) == 0 &&
                   ::fsync(outputFd) == 0;
    if (::close(outputFd) != 0) success = false;

    if (success) {
        // linkat is deliberately used instead of rename: it refuses to replace
        // any file that appeared at the digest-derived destination.
        success = ::linkat(directoryFd, temporaryName.constData(), directoryFd,
                           targetName.constData(), 0) == 0;
    }
    ::unlinkat(directoryFd, temporaryName.constData(), 0);
    if (success) ::fsync(directoryFd);
    ::close(directoryFd);
    return success;
}

int installCore(const QString &sourcePath, const QByteArray &expectedDigest,
                const QString &selfPath, uid_t callerUid) {
    if (::geteuid() != 0 || callerUid == 0 || expectedDigest.size() != 64 ||
        !QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(
            QString::fromLatin1(expectedDigest)).hasMatch()) {
        std::cerr << "Invalid privileged helper invocation.\n";
        return 2;
    }

    const QFileInfo selfInfo(selfPath);
    const QFile::Permissions writableByOthers = QFile::WriteGroup | QFile::WriteOther;
    if (!selfInfo.isFile() || !selfInfo.isExecutable() ||
        (selfInfo.ownerId() != 0 && selfInfo.ownerId() != callerUid) ||
        (selfInfo.permissions() & writableByOthers)) {
        std::cerr << "Refusing an unsafe installer binary.\n";
        return 3;
    }
    const QByteArray nativeSelf = QFile::encodeName(selfPath);
    const int selfFd = ::open(nativeSelf.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (selfFd < 0 || hasCapabilities(selfFd)) {
        if (selfFd >= 0) ::close(selfFd);
        std::cerr << "Refusing an installer with file capabilities.\n";
        return 3;
    }
    struct stat selfStat {};
    const bool safeSelfMode = ::fstat(selfFd, &selfStat) == 0 &&
                              !(selfStat.st_mode & (S_ISUID | S_ISGID));
    ::close(selfFd);
    if (!safeSelfMode) {
        std::cerr << "Refusing a set-ID installer binary.\n";
        return 3;
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString canonicalSource = sourceInfo.canonicalFilePath();
    if (sourcePath.isEmpty() || !QFileInfo(sourcePath).isAbsolute() || canonicalSource != sourcePath) {
        std::cerr << "Refusing a non-canonical core path.\n";
        return 4;
    }
    const QString helperDirectory = QFileInfo(selfPath).canonicalPath();
    const QString bundledCore = helperDirectory.isEmpty()
        ? QString() : QFileInfo(QDir(helperDirectory).filePath(QStringLiteral("beaxty-core"))).canonicalFilePath();
    const QString developmentCore = helperDirectory.isEmpty()
        ? QString() : QFileInfo(QDir(helperDirectory).filePath(QStringLiteral("../bin/beaxty-core"))).canonicalFilePath();
    if (canonicalSource != bundledCore && canonicalSource != developmentCore) {
        std::cerr << "The privileged helper accepts only the core bundled beside this application.\n";
        return 4;
    }
    const QByteArray nativeSource = QFile::encodeName(canonicalSource);
    const int sourceFd = ::open(nativeSource.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (sourceFd < 0) {
        std::cerr << "Could not open the bundled core safely.\n";
        return 5;
    }
    struct stat sourceStat {};
    if (!sourceFileIsSafe(sourceFd, callerUid, &sourceStat)) {
        ::close(sourceFd);
        std::cerr << "Refusing an unsafe bundled core.\n";
        return 6;
    }
    if (hashFileDescriptor(sourceFd) != expectedDigest) {
        ::close(sourceFd);
        std::cerr << "The bundled core digest changed before authorization.\n";
        return 7;
    }

    const QFileInfo libraryInfo(QStringLiteral("/usr/lib"));
    const QString libraryDirectory = libraryInfo.canonicalFilePath();
    if (libraryDirectory.isEmpty() || !libraryInfo.isDir() ||
        !trustedRootDirectoryChain(libraryDirectory)) {
        ::close(sourceFd);
        std::cerr << "The system library directory is not trusted.\n";
        return 8;
    }
    const QString coreRoot = QDir(libraryDirectory).filePath(QStringLiteral("beaxty-vpn"));
    if (!ensureRootDirectory(coreRoot, 0755)) {
        ::close(sourceFd);
        std::cerr << "Could not safely create the system core directory.\n";
        return 9;
    }

    const QString userDirectory = QDir(coreRoot).filePath(QString::number(callerUid));
    if (!ensureRootDirectory(userDirectory, 0700)) {
        ::close(sourceFd);
        std::cerr << "Could not safely create the per-user core directory.\n";
        return 10;
    }

    const QString setfaclPath = trustedSystemExecutable({QStringLiteral("/usr/bin/setfacl"),
                                                          QStringLiteral("/bin/setfacl")});
    const QString getfaclPath = trustedSystemExecutable({QStringLiteral("/usr/bin/getfacl"),
                                                          QStringLiteral("/bin/getfacl")});
    const QString setcapPath = trustedSystemExecutable({QStringLiteral("/usr/sbin/setcap"),
                                                         QStringLiteral("/sbin/setcap"),
                                                         QStringLiteral("/usr/bin/setcap")});
    const QString getcapPath = trustedSystemExecutable({QStringLiteral("/usr/sbin/getcap"),
                                                         QStringLiteral("/sbin/getcap"),
                                                         QStringLiteral("/usr/bin/getcap")});
    if (setfaclPath.isEmpty() || getfaclPath.isEmpty() || setcapPath.isEmpty() || getcapPath.isEmpty()) {
        ::close(sourceFd);
        std::cerr << "Polkit setup requires acl and libcap utilities.\n";
        return 11;
    }

    const QByteArray uidText = QByteArray::number(callerUid);
    const QByteArray ownerOnlyAcl = QByteArrayLiteral("user::rwx\ngroup::---\nother::---");
    const QByteArray exactUserAcl = QByteArrayLiteral("user::rwx\nuser:") + uidText +
        QByteArrayLiteral(":--x\ngroup::---\nmask::--x\nother::---");
    if (!directoryHasAcl(getfaclPath, userDirectory, ownerOnlyAcl) &&
        !directoryHasAcl(getfaclPath, userDirectory, exactUserAcl)) {
        ::close(sourceFd);
        std::cerr << "The existing per-user directory has unexpected ACL entries.\n";
        return 12;
    }
    if (!runTool(setfaclPath,
                 {QStringLiteral("-m"), QStringLiteral("u:%1:--x,m::--x").arg(QString::fromLatin1(uidText)),
                  QStringLiteral("--"), userDirectory}) ||
        !directoryHasAcl(getfaclPath, userDirectory, exactUserAcl)) {
        ::close(sourceFd);
        std::cerr << "Could not restrict core access to the current user.\n";
        return 13;
    }

    const QString targetPath = QDir(userDirectory).filePath(
        QStringLiteral("beaxty-vpn-core-%1").arg(QString::fromLatin1(expectedDigest)));
    const QFileInfo targetInfo(targetPath);
    bool installedByThisCall = false;
    if (targetInfo.exists() || targetInfo.isSymLink()) {
        if (targetInfo.isSymLink() || !hashPathMatches(targetPath, expectedDigest)) {
            ::close(sourceFd);
            std::cerr << "A non-matching file already exists at the core destination.\n";
            return 14;
        }
    } else {
        if (!copyCoreWithoutOverwrite(sourceFd, userDirectory, targetPath, expectedDigest)) {
            ::close(sourceFd);
            std::cerr << "Could not install the verified core copy.\n";
            return 15;
        }
        installedByThisCall = true;
    }
    ::close(sourceFd);

    if (!hashPathMatches(targetPath, expectedDigest) ||
        !runTool(setcapPath, {QStringLiteral("cap_net_admin=ep"), targetPath})) {
        if (installedByThisCall) QFile::remove(targetPath);
        std::cerr << "Could not assign the narrow TUN capability.\n";
        return 16;
    }

    QByteArray capabilityOutput;
    const QByteArray expectedCapability = QFile::encodeName(targetPath) +
                                         QByteArrayLiteral(" cap_net_admin=ep");
    if (!runTool(getcapPath, {targetPath}, &capabilityOutput) ||
        capabilityOutput != expectedCapability || !hashPathMatches(targetPath, expectedDigest)) {
        runTool(setcapPath, {QStringLiteral("-r"), targetPath});
        if (installedByThisCall) QFile::remove(targetPath);
        std::cerr << "The installed core failed capability verification.\n";
        return 17;
    }

    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 4 || args.at(1) != QStringLiteral("--install-core")) {
        std::cerr << "Usage: beaxty-vpn-privileged-helper --install-core <core-path> <sha256>\n";
        return 2;
    }
    const QString callerUidText = qEnvironmentVariable("PKEXEC_UID");
    if (!QRegularExpression(QStringLiteral("^[0-9]{1,10}$")).match(callerUidText).hasMatch()) {
        std::cerr << "The Polkit caller UID is missing or invalid.\n";
        return 2;
    }
    bool uidOk = false;
    const qulonglong parsedUid = callerUidText.toULongLong(&uidOk, 10);
    if (!uidOk || parsedUid > std::numeric_limits<uid_t>::max()) {
        std::cerr << "The Polkit caller UID is out of range.\n";
        return 2;
    }
    return installCore(args.at(2), args.at(3).toLatin1(), app.applicationFilePath(),
                       static_cast<uid_t>(parsedUid));
}
