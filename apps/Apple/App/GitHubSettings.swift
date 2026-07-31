import Foundation
import IssuesKit
import Security

/// The GitHub personal access token, stored in the Keychain.
///
/// Owner and repository are **not** here: they are per-project coordinates and live in the
/// document (`model.integrations.github`), edited in Project Settings. The token stays in the
/// Keychain and is never written into an `.issues` file, because those files are committed to
/// project repositories.
///
/// One item per repository. A token issued for one repository must never be sent to another, so
/// every stored item is keyed by the account returned from ``account(for:)`` and a document only
/// ever reads back the token saved against its own coordinates.
enum GitHubKeychain {
    private static let service = "IHaveIssues-GitHubToken"

    /// The Keychain account that scopes a token to one repository: `"<owner>/<repository>"`,
    /// trimmed and lowercased so the same repository always resolves to the same item.
    ///
    /// Returns `nil` when the integration is absent or either coordinate is blank — there is no
    /// repository to scope to, so there is nothing to save, load, or delete.
    static func account(for integration: GitHubIntegration?) -> String? {
        guard let integration else { return nil }
        let owner = integration.owner.trimmingCharacters(in: .whitespaces).lowercased()
        let repository = integration.repository.trimmingCharacters(in: .whitespaces).lowercased()
        guard !owner.isEmpty, !repository.isEmpty else { return nil }
        return "\(owner)/\(repository)"
    }

    /// The token is only read while the user is actively syncing, so it does not need to be
    /// available when the device is locked — hence `WhenUnlocked`. `ThisDeviceOnly` additionally
    /// keeps it out of encrypted backups and stops it migrating to another device.
    ///
    /// `kSecUseDataProtectionKeychain` is required for `kSecAttrAccessible` to take effect on
    /// macOS: without it macOS uses the legacy file-based keychain, which ignores that attribute.
    /// It has to be set on `load` and `delete` too, because the item can only be found in the
    /// keychain it was added to.
    static func save(_ token: String, for account: String) {
        let data = Data(token.utf8)
        let query: [CFString: Any] = [
            kSecClass: kSecClassGenericPassword,
            kSecAttrService: service,
            kSecAttrAccount: account,
            kSecAttrAccessible: kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            kSecUseDataProtectionKeychain: true,
            kSecValueData: data
        ]
        SecItemDelete(query as CFDictionary)
        SecItemAdd(query as CFDictionary, nil)
    }

    static func load(for account: String) -> String? {
        let query: [CFString: Any] = [
            kSecClass: kSecClassGenericPassword,
            kSecAttrService: service,
            kSecAttrAccount: account,
            kSecUseDataProtectionKeychain: true,
            kSecReturnData: kCFBooleanTrue!,
            kSecMatchLimit: kSecMatchLimitOne
        ]
        var item: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess,
              let data = item as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    /// Whether a token is stored for `account`, without reading it back. `kSecReturnData` is
    /// omitted deliberately: callers that only need to know an item exists — the sync sheet, so it
    /// can offer to replace or remove one — must never hold the secret to answer that question.
    static func hasToken(for account: String) -> Bool {
        let query: [CFString: Any] = [
            kSecClass: kSecClassGenericPassword,
            kSecAttrService: service,
            kSecAttrAccount: account,
            kSecUseDataProtectionKeychain: true,
            kSecMatchLimit: kSecMatchLimitOne
        ]
        return SecItemCopyMatching(query as CFDictionary, nil) == errSecSuccess
    }

    static func delete(for account: String) {
        let query: [CFString: Any] = [
            kSecClass: kSecClassGenericPassword,
            kSecAttrService: service,
            kSecAttrAccount: account,
            kSecUseDataProtectionKeychain: true
        ]
        SecItemDelete(query as CFDictionary)
    }
}
