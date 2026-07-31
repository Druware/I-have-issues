/*
 * KWalletTokenStore.cpp
 *
 * VERIFY: (whole file) none of this has been compiled -- no KDE Frameworks were
 * available. Check on a real KDE system:
 *   - the include is <KWallet> and the class is KWallet::Wallet, provided by the
 *     KF6::Wallet target from find_package(KF6 COMPONENTS Wallet);
 *   - KWallet::Wallet::openWallet(const QString &, WId, OpenType) returns a
 *     heap Wallet * that the CALLER owns, and nullptr on failure;
 *   - KWallet::Wallet::NetworkWallet() is the right wallet for stored
 *     credentials (LocalWallet() is the alternative on some setups);
 *   - writePassword / readPassword / removeEntry return 0 on success and
 *     non-zero on failure (this is the KWallet convention, NOT a bool);
 *   - hasEntry(), hasFolder(), createFolder() and setFolder() return bool;
 *   - the FIRST call may block on a modal unlock prompt. Everything here must
 *     therefore run on the GUI thread in response to a user action -- see the
 *     header.
 */
#include "KWalletTokenStore.h"

#include <QString>

#include <KWallet>

namespace ihaveissues
{

namespace
{

/*! The Apple app's kSecAttrService value, reused verbatim as the folder name.
 *
 *  Keeping the same string across ports means a user who reads their wallet by
 *  hand sees the same label they would see in Keychain Access.
 */
const char *const kFolderName = "IHaveIssues-GitHubToken";

} // unnamed namespace

KWalletTokenStore::KWalletTokenStore(WId windowId)
    : m_windowId(windowId)
    , m_wallet(nullptr)
{
}

KWalletTokenStore::~KWalletTokenStore()
{
    delete m_wallet;
}

bool KWalletTokenStore::ensureFolder()
{
    const QString folder = QString::fromLatin1(kFolderName);

    if (m_wallet != nullptr && m_wallet->isOpen()) {
        return m_wallet->setFolder(folder);
    }

    // A wallet that was open and has since closed is useless; drop it and retry.
    delete m_wallet;
    m_wallet = nullptr;

    if (!KWallet::Wallet::isEnabled()) {
        return false;
    }

    m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(),
                                           m_windowId,
                                           KWallet::Wallet::Synchronous);
    if (m_wallet == nullptr || !m_wallet->isOpen()) {
        delete m_wallet;
        m_wallet = nullptr;
        return false;
    }

    if (!m_wallet->hasFolder(folder) && !m_wallet->createFolder(folder)) {
        return false;
    }
    return m_wallet->setFolder(folder);
}

bool KWalletTokenStore::Save(const std::string &account, const std::string &token)
{
    if (account.empty() || token.empty()) {
        return false;
    }
    if (!ensureFolder()) {
        return false;
    }
    // writePassword replaces any existing entry, so this is already an upsert.
    return m_wallet->writePassword(QString::fromStdString(account),
                                   QString::fromStdString(token)) == 0;
}

bool KWalletTokenStore::Load(const std::string &account, std::string &outToken)
{
    if (account.empty()) {
        return false;
    }
    if (!ensureFolder()) {
        return false;
    }

    QString value;
    if (m_wallet->readPassword(QString::fromStdString(account), value) != 0) {
        return false;
    }
    if (value.isEmpty()) {
        return false;
    }
    outToken = value.toStdString();
    return true;
}

bool KWalletTokenStore::HasToken(const std::string &account)
{
    if (account.empty()) {
        return false;
    }
    if (!ensureFolder()) {
        return false;
    }
    // The TokenStore contract in the good case: existence is answered without the
    // secret ever being decrypted, let alone returned.
    return m_wallet->hasEntry(QString::fromStdString(account));
}

bool KWalletTokenStore::Remove(const std::string &account)
{
    if (account.empty()) {
        return false;
    }
    if (!ensureFolder()) {
        return false;
    }
    if (!m_wallet->hasEntry(QString::fromStdString(account))) {
        return false;
    }
    return m_wallet->removeEntry(QString::fromStdString(account)) == 0;
}

} // namespace ihaveissues
