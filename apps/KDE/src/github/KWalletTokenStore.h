/*
 * KWalletTokenStore.h -- issueskit::TokenStore backed by KWallet.
 *
 * KWallet is KDE's analogue of Apple's Keychain, Haiku's BKeyStore and GNOME's
 * libsecret. Only the backend differs between the ports; the account key
 * derivation is shared and lives in issueskit::TokenStore::AccountFor(). This
 * class must never derive that key itself -- if two desktops disagree about the
 * key, a token saved in one app is invisible to the other opening the same
 * document.
 *
 * Owner and repository are NOT stored here: they are per-project coordinates
 * that live in the .issues document and are edited in Project Settings. Only the
 * token lives in the wallet, and it is never written into a document, because
 * those files are committed to project repositories.
 *
 * THREADING. Opening a wallet is a D-Bus round trip that may raise a modal
 * unlock prompt, and KWallet::Wallet is a QObject with thread affinity. Every
 * method here must therefore be called from the GUI thread, in response to a
 * user action -- never from the sync worker thread and never from a constructor
 * that runs while a window is still being built. GitHubSyncDialog reads the
 * token on the GUI thread and copies it into the worker's context before the
 * thread starts.
 */
#ifndef IHAVEISSUES_KWALLET_TOKEN_STORE_H
#define IHAVEISSUES_KWALLET_TOKEN_STORE_H

#include <string>

#include <qwindowdefs.h> // WId

#include <issueskit/TokenStore.h>

namespace KWallet
{
class Wallet;
}

namespace ihaveissues
{

class KWalletTokenStore : public issueskit::TokenStore
{
public:
    /*! \param windowId The window the unlock prompt should be transient for.
     *      0 is accepted and simply produces an unparented prompt.
     */
    explicit KWalletTokenStore(WId windowId = 0);
    ~KWalletTokenStore() override;

    //! Upserts the token for \a account. False when the wallet refused it.
    bool Save(const std::string &account, const std::string &token) override;

    //! Reads the token back, or returns false when none is stored.
    bool Load(const std::string &account, std::string &outToken) override;

    /*! Whether a token exists for \a account.
     *
     *  KWallet::Wallet::hasEntry() answers this without decrypting, so unlike
     *  the Haiku port this implementation never even reads the secret.
     */
    bool HasToken(const std::string &account) override;

    bool Remove(const std::string &account) override;

private:
    //! Opens the wallet if needed and selects this app's folder.
    bool ensureFolder();

    WId m_windowId;
    KWallet::Wallet *m_wallet;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_KWALLET_TOKEN_STORE_H
