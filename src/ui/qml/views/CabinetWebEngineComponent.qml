// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtWebEngine

WebEngineView {
    id: webView
    anchors.fill: parent

    signal deepLinkTriggered(string url)
    signal externalUrlTriggered(string url)

    property bool hasError: false

    url: "https://cabinet.beaxty.com"

    // Extreme Security Configuration
    settings.javascriptCanOpenWindows: false
    settings.pluginsEnabled: false
    settings.localStorageEnabled: true
    settings.localContentCanAccessRemoteUrls: false
    settings.localContentCanAccessFileUrls: false
    settings.allowRunningInsecureContent: false
    settings.screenCaptureEnabled: false
    settings.webRTCPublicInterfacesOnly: true
    settings.forceDarkMode: Theme.isDark

    // Disable geolocation, microphone, camera and desktop media permissions
    onPermissionRequested: function(permissionRequest) {
        console.warn("[CabinetWebEngine] Denied permission request:", permissionRequest.type);
        permissionRequest.deny();
    }

    onAuthenticationDialogRequested: function(request) {
        request.accepted = true;
        request.dialogReject();
    }

    function managerAvailable() {
        return typeof deepLinkManager !== "undefined" && deepLinkManager !== null;
    }

    function rejectNavigation(request) {
        request.action = WebEngineNavigationRequest.IgnoreRequest;
    }

    function acceptNavigation(request) {
        request.action = WebEngineNavigationRequest.AcceptRequest;
    }

    // Restrict top-level pages to the exact HTTPS cabinet origin. Only direct
    // user clicks may hand links or beaxty:// deep links to the operating system.
    onNavigationRequested: function(request) {
        var reqUrl = request.url ? request.url.toString() : "";
        var isBeaxty = managerAvailable() && deepLinkManager.isBeaxtyUrl(reqUrl);

        if (isBeaxty) {
            rejectNavigation(request);
            if (request.isMainFrame &&
                    request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation) {
                webView.deepLinkTriggered(reqUrl);
            }
            return;
        }

        if (managerAvailable() && deepLinkManager.isTrustedCabinetUrl(reqUrl)) {
            acceptNavigation(request);
            return;
        }

        rejectNavigation(request);
        if (request.isMainFrame &&
                request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation &&
                managerAvailable() && deepLinkManager.isAllowedExternalUrl(reqUrl)) {
            webView.externalUrlTriggered(reqUrl);
        }
    }

    // Ignore automatic popups, even when their destination is trusted.
    onNewWindowRequested: function(request) {
        var reqUrl = request.requestedUrl ? request.requestedUrl.toString() : "";
        if (!request.userInitiated || !managerAvailable()) return;

        if (deepLinkManager.isBeaxtyUrl(reqUrl)) {
            webView.deepLinkTriggered(reqUrl);
            return;
        }

        if (deepLinkManager.isTrustedCabinetUrl(reqUrl)) {
            // Open in same view instead of separate window
            webView.url = request.requestedUrl;
        } else if (deepLinkManager.isAllowedExternalUrl(reqUrl)) {
            webView.externalUrlTriggered(reqUrl);
        }
    }

    onLoadingChanged: function(loadRequest) {
        if (loadRequest.status === WebEngineView.LoadFailedStatus) {
            // Ignore abort errors from intercepted requests
            if (loadRequest.errorCode !== -3) { // net::ERR_ABORTED
                console.warn("[CabinetWebEngine] Load failed:", loadRequest.errorString, "code:", loadRequest.errorCode);
                webView.hasError = true;
            }
        } else if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
            webView.hasError = false;
        }
    }
}
