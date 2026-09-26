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

    // Disable geolocation, microphone, camera and desktop media permissions
    onPermissionRequested: function(permissionRequest) {
        console.warn("[CabinetWebEngine] Denied permission request:", permissionRequest.type);
        permissionRequest.deny();
    }

    onAuthenticationDialogRequested: function(request) {
        request.accepted = true;
        request.dialogReject();
    }

    function parseUrlParts(urlStr) {
        var str = urlStr ? urlStr.toString() : "";
        var match = str.match(/^([a-zA-Z][a-zA-Z0-9+.-]*):\/\/([^\/\?#]+)/);
        if (match) {
            var authority = match[2].toLowerCase();
            // Strip userinfo before parsing the host. Otherwise
            // https://cabinet.beaxty.com:443@untrusted.example/ would be
            // mistaken for a trusted cabinet origin.
            var at = authority.lastIndexOf("@");
            if (at >= 0) authority = authority.substring(at + 1);
            var hostOnly = authority;
            if (hostOnly.charAt(0) === "[") {
                var closing = hostOnly.indexOf("]");
                if (closing >= 0) hostOnly = hostOnly.substring(1, closing);
            } else {
                var colon = hostOnly.lastIndexOf(":");
                if (colon >= 0 && hostOnly.substring(colon + 1).match(/^\d*$/)) {
                    hostOnly = hostOnly.substring(0, colon);
                }
            }
            return { scheme: match[1].toLowerCase(), host: hostOnly };
        }
        return { scheme: "", host: "" };
    }

    function isAllowedExternalUrl(urlStr) {
        var scheme = parseUrlParts(urlStr).scheme;
        return scheme === "https" || scheme === "http";
    }

    // Intercept navigation requests to enforce Strict Whitelist
    onNavigationRequested: function(request) {
        var reqUrl = request.url ? request.url.toString() : "";
        var parts = parseUrlParts(reqUrl);

        // 1. Intercept beaxty:// deeplinks
        if (parts.scheme === "beaxty" || reqUrl.startsWith("beaxty://")) {
            if (typeof request.reject === "function") request.reject();
            else request.action = WebEngineNavigationRequest.IgnoreRequest;
            webView.deepLinkTriggered(reqUrl);
            return;
        }

        // 2. Strict Origin Whitelist: allow only https://cabinet.beaxty.com and https://*.beaxty.com
        var isTrustedHost = (parts.host === "cabinet.beaxty.com" || parts.host.endsWith(".beaxty.com"));
        var isSecureScheme = (parts.scheme === "https");

        if (isTrustedHost && isSecureScheme) {
            if (typeof request.accept === "function") request.accept();
            else request.action = WebEngineNavigationRequest.AcceptRequest;
        } else {
            // External URL: reject in embedded browser and open in external system browser
            if (typeof request.reject === "function") request.reject();
            else request.action = WebEngineNavigationRequest.IgnoreRequest;
            console.log("[CabinetWebEngine] External URL redirected to system browser:",
                         parts.scheme + "://" + parts.host);
            if (isAllowedExternalUrl(reqUrl)) webView.externalUrlTriggered(reqUrl);
        }
    }

    // Intercept target="_blank" or window.open requests
    onNewWindowRequested: function(request) {
        var reqUrl = request.requestedUrl ? request.requestedUrl.toString() : "";
        var parts = parseUrlParts(reqUrl);

        if (parts.scheme === "beaxty" || reqUrl.startsWith("beaxty://")) {
            webView.deepLinkTriggered(reqUrl);
            return;
        }

        var isTrustedHost = (parts.host === "cabinet.beaxty.com" || parts.host.endsWith(".beaxty.com"));

        if (isTrustedHost && parts.scheme === "https") {
            // Open in same view instead of separate window
            webView.url = request.requestedUrl;
        } else if (isAllowedExternalUrl(reqUrl)) {
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
