package com.druware.ihaveissues.github

import android.content.Context
import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import com.druware.issueskit.GitHubIntegration
import java.security.GeneralSecurityException
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * Where the GitHub personal access token lives.
 *
 * Owner and repository are **not** here: they are per-project coordinates that belong to the
 * document. The token never touches an `.issues` file, because those files are committed to project
 * repositories.
 *
 * One entry per repository, keyed by [gitHubTokenAccount], so a token issued for one repository is
 * never sent to another.
 *
 * The interface exists so the view model can be tested on the JVM: the real implementation needs the
 * Android keystore, which only exists on a device.
 */
interface GitHubTokenStore {

    /** Stores [token] against [account], replacing whatever was there. */
    fun save(token: String, account: String)

    /** The token for [account], or `null` when there is none this build can read. */
    fun load(account: String): String?

    /**
     * Whether a token is stored for [account], *without* reading it.
     *
     * The sync screen only needs to know one exists so it can offer to replace or remove it. It must
     * never hold the secret to answer that question.
     */
    fun hasToken(account: String): Boolean

    fun delete(account: String)
}

/**
 * The storage key that scopes a token to one repository: `"<owner>/<repository>"`, trimmed and
 * lowercased so the same repository always resolves to the same entry.
 *
 * `null` when the integration is absent or either coordinate is blank — there is no repository to
 * scope to, so there is nothing to save, load or delete.
 */
fun gitHubTokenAccount(integration: GitHubIntegration?): String? {
    val owner = integration?.owner?.trim()?.lowercase().orEmpty()
    val repository = integration?.repository?.trim()?.lowercase().orEmpty()
    if (owner.isEmpty() || repository.isEmpty()) return null
    return "$owner/$repository"
}

/**
 * The [GitHubTokenStore] backed by the Android keystore.
 *
 * The token is sealed with AES-256-GCM under a key that is generated inside the keystore and can
 * never be exported from it; only the ciphertext lands in a private preferences file. That is the
 * platform equivalent of the Keychain item the Apple app uses.
 *
 * `androidx.security:security-crypto` (`EncryptedSharedPreferences`) would do the same job with less
 * code, but Jetpack Security Crypto is deprecated and no longer maintained, so the platform APIs it
 * wraps are used directly rather than taking on a dependency with no future.
 *
 * On API 28 and above the key additionally requires an unlocked device, matching the Apple app's
 * `kSecAttrAccessibleWhenUnlockedThisDeviceOnly`: the token is only ever needed while the user is
 * actively syncing in the foreground.
 */
class KeystoreGitHubTokenStore(context: Context) : GitHubTokenStore {

    private val preferences =
        context.applicationContext.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)

    override fun save(token: String, account: String) {
        val cipher = Cipher.getInstance(TRANSFORMATION)
        cipher.init(Cipher.ENCRYPT_MODE, secretKey())
        val sealed = cipher.iv + cipher.doFinal(token.toByteArray(Charsets.UTF_8))
        preferences.edit()
            .putString(account, Base64.encodeToString(sealed, Base64.NO_WRAP))
            .apply()
    }

    override fun load(account: String): String? {
        val stored = preferences.getString(account, null) ?: return null
        return try {
            val sealed = Base64.decode(stored, Base64.NO_WRAP)
            val cipher = Cipher.getInstance(TRANSFORMATION)
            cipher.init(
                Cipher.DECRYPT_MODE,
                secretKey(),
                GCMParameterSpec(TAG_LENGTH_BITS, sealed, 0, IV_LENGTH_BYTES),
            )
            String(
                cipher.doFinal(sealed, IV_LENGTH_BYTES, sealed.size - IV_LENGTH_BYTES),
                Charsets.UTF_8,
            )
        } catch (_: GeneralSecurityException) {
            // The key is gone or the device is locked. The entry is left alone rather than deleted:
            // a locked device is temporary, and saving a token again overwrites it anyway.
            null
        } catch (_: IllegalArgumentException) {
            null
        }
    }

    override fun hasToken(account: String): Boolean = preferences.contains(account)

    override fun delete(account: String) {
        preferences.edit().remove(account).apply()
    }

    /** The one keystore key every entry is sealed with, generated on first use. */
    private fun secretKey(): SecretKey {
        val keyStore = KeyStore.getInstance(ANDROID_KEY_STORE).apply { load(null) }
        (keyStore.getEntry(KEY_ALIAS, null) as? KeyStore.SecretKeyEntry)?.let { return it.secretKey }

        val generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, ANDROID_KEY_STORE)
        val spec = KeyGenParameterSpec.Builder(
            KEY_ALIAS,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT,
        )
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(KEY_SIZE_BITS)
            // A fresh IV per encryption; reusing one under GCM would leak the plaintext.
            .setRandomizedEncryptionRequired(true)
            .apply {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) setUnlockedDeviceRequired(true)
            }
            .build()
        generator.init(spec)
        return generator.generateKey()
    }

    private companion object {
        const val ANDROID_KEY_STORE = "AndroidKeyStore"
        const val KEY_ALIAS = "IHaveIssues-GitHubToken"
        const val PREFERENCES_NAME = "github-tokens"
        const val TRANSFORMATION = "AES/GCM/NoPadding"
        const val KEY_SIZE_BITS = 256
        const val IV_LENGTH_BYTES = 12
        const val TAG_LENGTH_BITS = 128
    }
}
