package com.druware.ihaveissues

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.druware.ihaveissues.document.SafDocumentStore
import com.druware.ihaveissues.github.KeystoreGitHubTokenStore
import com.druware.ihaveissues.ui.IssuesApp
import com.druware.ihaveissues.ui.IssuesViewModel
import com.druware.ihaveissues.ui.theme.IHaveIssuesTheme

class MainActivity : ComponentActivity() {

    private val viewModel: IssuesViewModel by viewModels {
        IssuesViewModel.factory(
            store = SafDocumentStore(applicationContext),
            tokens = KeystoreGitHubTokenStore(applicationContext),
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        // Only on a cold start: on a configuration change the view model already holds the document,
        // and reopening the launch intent's URI would throw away unsaved edits.
        if (savedInstanceState == null) openDocumentFrom(intent)
        setContent {
            IHaveIssuesTheme {
                val state by viewModel.state.collectAsStateWithLifecycle()
                IssuesApp(state = state, viewModel = viewModel, modifier = Modifier.fillMaxSize())
            }
        }
    }

    /** Handles a second `.issues` file being opened while the app is already running. */
    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        openDocumentFrom(intent)
    }

    /** Opens the document a file manager handed us, if this intent carries one. */
    private fun openDocumentFrom(intent: Intent?) {
        if (intent?.action != Intent.ACTION_VIEW) return
        val uri = intent.data ?: return
        viewModel.openDocument(uri.toString())
    }
}
