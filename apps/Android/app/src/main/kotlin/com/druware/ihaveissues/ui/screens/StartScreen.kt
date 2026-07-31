package com.druware.ihaveissues.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.druware.ihaveissues.R

/**
 * What the app shows with no document open.
 *
 * Apple gets this for free from `DocumentGroup`'s browser; Android has no equivalent scene, so the
 * two entry points into the Storage Access Framework are presented explicitly.
 */
@Composable
fun StartScreen(
    isLoading: Boolean,
    onNewDocument: () -> Unit,
    onOpenDocument: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(modifier = modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Column(
            modifier = Modifier
                .widthIn(max = 400.dp)
                .padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Icon(
                painter = painterResource(R.drawable.ic_document),
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
                modifier = Modifier.size(56.dp),
            )
            Text(
                text = stringResource(R.string.app_name),
                style = MaterialTheme.typography.headlineSmall,
            )
            Text(
                text = "Issues live in a single .issues file you own. Create one, or open a file " +
                    "you already have.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                textAlign = TextAlign.Center,
            )
            if (isLoading) {
                CircularProgressIndicator()
            } else {
                Button(onClick = onNewDocument, modifier = Modifier.fillMaxWidth()) {
                    Text("New Document")
                }
                OutlinedButton(onClick = onOpenDocument, modifier = Modifier.fillMaxWidth()) {
                    Text("Open Document…")
                }
            }
        }
    }
}
