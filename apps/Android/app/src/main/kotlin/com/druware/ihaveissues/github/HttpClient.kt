package com.druware.ihaveissues.github

import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** One outbound HTTP call. Bodies are text because every GitHub payload here is JSON. */
data class HttpRequest(
    val method: String,
    val url: String,
    val headers: Map<String, String> = emptyMap(),
    val body: String? = null,
)

/** What came back. Non-2xx responses carry a body too — that is where GitHub puts `message`. */
data class HttpResponse(
    val statusCode: Int,
    val body: String,
    val headers: Map<String, List<String>> = emptyMap(),
) {
    /** The first value of [name], matched case-insensitively as HTTP header names are. */
    fun header(name: String): String? = headers.entries
        .firstOrNull { it.key.equals(name, ignoreCase = true) }
        ?.value
        ?.firstOrNull()
}

/**
 * Every byte [GitHubSyncService] sends or receives goes through here.
 *
 * The interface exists so the sync service can be exercised on the JVM with no network and no
 * device: the real implementation opens a socket, the test implementation answers from a table.
 */
fun interface HttpClient {

    suspend fun send(request: HttpRequest): HttpResponse
}

/**
 * The [HttpClient] backed by the platform's own stack.
 *
 * `HttpURLConnection` is chosen over an HTTP library because the interface above already supplies
 * the seam that makes the service testable, so a library would buy only a socket implementation the
 * platform ships anyway. Android's `HttpURLConnection` is OkHttp underneath and accepts `PATCH`,
 * which the desktop JDK's implementation refuses.
 */
class UrlConnectionHttpClient : HttpClient {

    override suspend fun send(request: HttpRequest): HttpResponse = withContext(Dispatchers.IO) {
        val connection = URL(request.url).openConnection() as HttpURLConnection
        try {
            connection.requestMethod = request.method
            connection.connectTimeout = CONNECT_TIMEOUT_MILLIS
            connection.readTimeout = READ_TIMEOUT_MILLIS
            request.headers.forEach { (name, value) -> connection.setRequestProperty(name, value) }
            if (request.body != null) {
                connection.doOutput = true
                connection.outputStream.use { it.write(request.body.toByteArray(Charsets.UTF_8)) }
            }

            val status = connection.responseCode
            // A 4xx/5xx body arrives on the error stream; reading only `inputStream` would throw
            // away the very message the user needs to see.
            val stream = if (status in 200..299) connection.inputStream else connection.errorStream
            val body = stream?.use { it.readBytes().toString(Charsets.UTF_8) }.orEmpty()
            // Read before disconnecting: the header map is only valid while the connection is open.
            val headers = connection.headerFields.orEmpty()
                .mapNotNull { (name, values) -> name?.let { it to values } }
                .toMap()
            HttpResponse(statusCode = status, body = body, headers = headers)
        } finally {
            connection.disconnect()
        }
    }

    private companion object {
        const val CONNECT_TIMEOUT_MILLIS = 15_000
        const val READ_TIMEOUT_MILLIS = 30_000
    }
}
