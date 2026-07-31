package com.druware.ihaveissues.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.DatePicker
import androidx.compose.material3.DatePickerDialog
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuAnchorType
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.rememberDatePickerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.druware.ihaveissues.R
import com.druware.ihaveissues.ui.IssueDraft
import com.druware.issueskit.IssueDate
import com.druware.issueskit.IssuePriority
import com.druware.issueskit.IssueStatus
import com.druware.issueskit.IssueType
import com.druware.issueskit.ResolutionKind
import java.time.Instant

/**
 * The add/edit form.
 *
 * Every field the Apple `IssueEditView` carries is here, in the same order. The screen is stateless:
 * it renders [draft] and reports edits through [onChange], so the view model owns the draft and it
 * survives rotation without any extra work.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun IssueEditScreen(
    draft: IssueDraft,
    onChange: ((IssueDraft) -> IssueDraft) -> Unit,
    onCancel: () -> Unit,
    onSave: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var showDatePicker by remember { mutableStateOf(false) }

    Surface(modifier = modifier.fillMaxSize()) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text(if (draft.isNew) "New Issue" else "Edit Issue") },
                    navigationIcon = {
                        IconButton(onClick = onCancel) {
                            Icon(painterResource(R.drawable.ic_close), contentDescription = "Cancel")
                        }
                    },
                    actions = { TextButton(onClick = onSave) { Text("Save") } },
                )
            },
        ) { innerPadding ->
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding)
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 16.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                FormSection("Summary")
                OutlinedTextField(
                    value = draft.title,
                    onValueChange = { value -> onChange { it.copy(title = value) } },
                    label = { Text("Title") },
                    modifier = Modifier.fillMaxWidth(),
                )
                EnumField("Type", draft.type, IssueType.entries, { it.displayName }) { value ->
                    onChange { it.copy(type = value) }
                }
                EnumField("Priority", draft.priority, IssuePriority.entries, { it.displayName }) { value ->
                    onChange { it.copy(priority = value) }
                }
                EnumField("Status", draft.status, IssueStatus.entries, { it.displayName }) { value ->
                    onChange { it.copy(status = value) }
                }
                // "None" is an explicit option: ResolutionKind has no default case, because inventing
                // a reason an issue closed would misreport why the work stopped.
                EnumField(
                    label = "Resolution",
                    selected = draft.resolutionKind,
                    options = listOf<ResolutionKind?>(null) + ResolutionKind.entries,
                    display = { it?.displayName ?: "None" },
                ) { value -> onChange { it.copy(resolutionKind = value) } }

                FormSection("Details")
                OutlinedTextField(
                    value = IssueDate.stringFrom(draft.reported),
                    onValueChange = {},
                    readOnly = true,
                    label = { Text("Reported") },
                    trailingIcon = {
                        TextButton(onClick = { showDatePicker = true }) { Text("Change") }
                    },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = draft.reportedBy,
                    onValueChange = { value -> onChange { it.copy(reportedBy = value) } },
                    label = { Text("Reported by") },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = draft.area,
                    onValueChange = { value -> onChange { it.copy(area = value) } },
                    label = { Text("Area") },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = draft.milestoneText,
                    onValueChange = { value -> onChange { it.copy(milestoneText = value) } },
                    label = { Text("Milestone") },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = draft.estimateText,
                    onValueChange = { value -> onChange { it.copy(estimateText = value) } },
                    label = { Text("Estimate") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    supportingText = { Text("Left blank, or dropped, when the value is not a number.") },
                    modifier = Modifier.fillMaxWidth(),
                )

                FormSection("Labels & People")
                OutlinedTextField(
                    value = draft.labelsText,
                    onValueChange = { value -> onChange { it.copy(labelsText = value) } },
                    label = { Text("Labels") },
                    supportingText = { Text("Separate multiple entries with commas.") },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = draft.assigneesText,
                    onValueChange = { value -> onChange { it.copy(assigneesText = value) } },
                    label = { Text("Assignees") },
                    supportingText = { Text("Separate multiple entries with commas.") },
                    modifier = Modifier.fillMaxWidth(),
                )

                MultilineField("Description", draft.description) { value ->
                    onChange { it.copy(description = value) }
                }
                MultilineField(
                    label = "Steps to Reproduce",
                    value = draft.stepsText,
                    supporting = "One step per line.",
                ) { value -> onChange { it.copy(stepsText = value) } }
                MultilineField("Environment", draft.environment) { value ->
                    onChange { it.copy(environment = value) }
                }
                MultilineField("Notes / Investigation", draft.notes) { value ->
                    onChange { it.copy(notes = value) }
                }
                MultilineField("Resolution", draft.resolution) { value ->
                    onChange { it.copy(resolution = value) }
                }
            }
        }
    }

    if (showDatePicker) {
        ReportedDatePicker(
            initial = draft.reported,
            onDismiss = { showDatePicker = false },
            onPick = { picked ->
                showDatePicker = false
                onChange { it.copy(reported = picked) }
            },
        )
    }
}

/**
 * The reported-date picker.
 *
 * `DatePickerState` works in UTC milliseconds, which is exactly what the format wants: `reported` is
 * a calendar day pinned to UTC midnight, so a user east of Greenwich must not see it shift a day.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ReportedDatePicker(initial: Instant, onDismiss: () -> Unit, onPick: (Instant) -> Unit) {
    val pickerState = rememberDatePickerState(initialSelectedDateMillis = initial.toEpochMilli())
    DatePickerDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(
                onClick = { pickerState.selectedDateMillis?.let { onPick(Instant.ofEpochMilli(it)) } },
            ) { Text("OK") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    ) {
        DatePicker(state = pickerState)
    }
}

@Composable
private fun FormSection(title: String) {
    Text(
        text = title,
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.primary,
        modifier = Modifier.padding(top = 8.dp),
    )
}

@Composable
private fun MultilineField(
    label: String,
    value: String,
    supporting: String? = null,
    onValueChange: (String) -> Unit,
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        label = { Text(label) },
        minLines = 3,
        supportingText = supporting?.let { { Text(it) } },
        modifier = Modifier.fillMaxWidth(),
    )
}

/** A read-only text field that opens a menu — Material 3's answer to a `Picker`. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun <T> EnumField(
    label: String,
    selected: T,
    options: List<T>,
    display: (T) -> String,
    onSelect: (T) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
        modifier = Modifier.fillMaxWidth(),
    ) {
        OutlinedTextField(
            value = display(selected),
            onValueChange = {},
            readOnly = true,
            label = { Text(label) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            modifier = Modifier
                .fillMaxWidth()
                .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryNotEditable),
        )
        ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            options.forEach { option ->
                DropdownMenuItem(
                    text = { Text(display(option)) },
                    onClick = {
                        expanded = false
                        onSelect(option)
                    },
                )
            }
        }
    }
}
