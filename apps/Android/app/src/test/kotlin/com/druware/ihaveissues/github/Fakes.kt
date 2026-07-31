package com.druware.ihaveissues.github

/**
 * An [HttpClient] that answers from a script instead of a socket.
 *
 * Every test in this module runs against this: nothing here ever opens a connection, so the suite
 * needs neither network access nor a device.
 */
internal class FakeHttpClient(
    private var responder: (HttpRequest) -> HttpResponse = { ok("{\"number\": 1}") },
) : HttpClient {

    val requests = mutableListOf<HttpRequest>()

    override suspend fun send(request: HttpRequest): HttpResponse {
        requests += request
        return responder(request)
    }

    fun respondWith(responder: (HttpRequest) -> HttpResponse) {
        this.responder = responder
    }

    /** The requests as `"METHOD /path"`, which is what routing assertions actually care about. */
    fun routes(): List<String> = requests.map { "${it.method} ${it.url.substringAfter(API_BASE)}" }

    companion object {
        const val API_BASE = "https://api.github.com"

        fun ok(body: String, headers: Map<String, List<String>> = emptyMap()) =
            HttpResponse(statusCode = 200, body = body, headers = headers)

        fun created(number: Int, htmlUrl: String? = null, updatedAt: String? = null): HttpResponse {
            val fields = buildList {
                add("\"number\": $number")
                htmlUrl?.let { add("\"html_url\": \"$it\"") }
                updatedAt?.let { add("\"updated_at\": \"$it\"") }
            }
            return HttpResponse(statusCode = 201, body = "{${fields.joinToString(", ")}}")
        }

        fun failure(status: Int, message: String? = null) = HttpResponse(
            statusCode = status,
            body = message?.let { "{\"message\": \"$it\"}" }.orEmpty(),
        )
    }
}

/** A [GitHubTokenStore] holding tokens in a map; the real one needs a device keystore. */
internal class FakeGitHubTokenStore : GitHubTokenStore {

    val tokens = mutableMapOf<String, String>()

    override fun save(token: String, account: String) {
        tokens[account] = token
    }

    override fun load(account: String): String? = tokens[account]

    override fun hasToken(account: String): Boolean = tokens.containsKey(account)

    override fun delete(account: String) {
        tokens.remove(account)
    }
}
