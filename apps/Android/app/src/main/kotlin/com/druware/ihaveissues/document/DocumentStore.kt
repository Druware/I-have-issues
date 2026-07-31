package com.druware.ihaveissues.document

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import androidx.core.net.toUri
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.io.IOException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Byte-level access to the document the user picked.
 *
 * The interface exists so the view model can be exercised on the JVM without a device: the real
 * implementation talks to the Storage Access Framework, the test implementation keeps bytes in a
 * map. URIs are carried as strings because that is the form that survives `SavedStateHandle`.
 */
interface DocumentStore {

    /** Reads the whole document. */
    suspend fun read(uri: String): ByteArray

    /** Replaces the whole document. Partial writes are never performed — the format is one file. */
    suspend fun write(uri: String, bytes: ByteArray)

    /** The document's file name for display, or `null` when the provider does not report one. */
    suspend fun displayName(uri: String): String?

    /**
     * Asks the system to remember read/write access to [uri] across restarts.
     *
     * Without this a picked document is only readable until the process dies, which would make the
     * "reopen last document" path fail on every cold start.
     */
    fun takePersistablePermission(uri: String)
}

/** The [DocumentStore] backed by the Storage Access Framework. */
class SafDocumentStore(private val context: Context) : DocumentStore {

    override suspend fun read(uri: String): ByteArray = withContext(Dispatchers.IO) {
        val parsed = uri.toUri()
        context.contentResolver.openInputStream(parsed)?.use { it.readBytes() }
            ?: throw FileNotFoundException("The document could not be opened for reading.")
    }

    override suspend fun write(uri: String, bytes: ByteArray) {
        withContext(Dispatchers.IO) {
            writeBlocking(uri.toUri(), bytes)
        }
    }

    private fun writeBlocking(parsed: Uri, bytes: ByteArray) {
        // "rwt" truncates first, which matters because the format is a whole-file rewrite: writing
        // a shorter document over a longer one in "w" mode leaves the old tail behind on providers
        // that do not truncate. Not every provider accepts "rwt", so fall back and truncate by hand.
        val descriptor = openDescriptor(parsed, "rwt") ?: openDescriptor(parsed, "w")
        ?: throw FileNotFoundException("The document could not be opened for writing.")
        descriptor.use { file ->
            FileOutputStream(file.fileDescriptor).use { stream ->
                stream.channel.truncate(0)
                stream.write(bytes)
                stream.flush()
                // Push the bytes to storage before the descriptor closes; an autosave that only
                // reached the page cache is not a save.
                runCatching { file.fileDescriptor.sync() }
            }
        }
    }

    override suspend fun displayName(uri: String): String? = withContext(Dispatchers.IO) {
        val projection = arrayOf(OpenableColumns.DISPLAY_NAME)
        runCatching {
            context.contentResolver.query(uri.toUri(), projection, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst() && !cursor.isNull(0)) cursor.getString(0) else null
            }
        }.getOrNull()
    }

    override fun takePersistablePermission(uri: String) {
        val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        // A file:// URI, or a provider that never offered a persistable grant, throws here. That is
        // not fatal — the document still works for this launch — so the failure is swallowed.
        runCatching { context.contentResolver.takePersistableUriPermission(uri.toUri(), flags) }
    }

    private fun openDescriptor(uri: Uri, mode: String) = try {
        context.contentResolver.openFileDescriptor(uri, mode)
    } catch (_: IllegalArgumentException) {
        null
    } catch (_: IOException) {
        null
    } catch (_: SecurityException) {
        null
    }
}
