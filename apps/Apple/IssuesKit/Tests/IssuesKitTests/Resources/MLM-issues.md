# Issues

This file tracks known issues for MailListManager — a .NET 10 API (`src/MailListManager.Api`),
a shared library and background poller (`src/MailListManager.Core`, `src/MailListManager.Infrastructure`,
`src/MailListManager.Worker`), a Vue 3 SPA (`web/`) served by hardened nginx, and a native
Swift/SwiftUI macOS client (`clients/MailListManagerMac/`). The web and API images are deployed as
sidecar containers on Azure Web App for Containers (App Service), per `deploy/README.md`. Use this
file to log defects, track their status, and record the fix. Add new entries under "Known Issues"
using the template below; keep IDs sequential and never reuse a retired one.

## Issue Template

```
### MLM-XXX — <title>

- **ID:** MLM-XXX
- **Severity:** Critical | High | Medium | Low
- **Status:** Open | In Progress | Resolved
- **Component:** <e.g. API / Infrastructure / Worker / Web / Swift Client / Deploy>

**Description**
<What is wrong and why it matters.>

**Steps to Reproduce**
<Concrete steps, or the code path that demonstrates the problem, with file:line references.>

**Proposed Fix**
<What would resolve it.>

**Notes**
<Anything else: related commits, workarounds, follow-ups.>
```

## Known Issues

### MLM-001 — Login endpoint never triggers the configured account lockout

- **ID:** MLM-001
- **Severity:** High
- **Status:** Open
- **Component:** API (Auth)

**Description**
`Program.cs` configures Identity lockout (`options.Lockout.MaxFailedAccessAttempts = 10;`,
`src/MailListManager.Api/Program.cs:36`) and `AuthController.Login` checks
`userManager.IsLockedOutAsync(user)` (`src/MailListManager.Api/Controllers/AuthController.cs:61`).
However, the login path authenticates with `UserManager.CheckPasswordAsync`
(`src/MailListManager.Api/Controllers/AuthController.cs:56`), which only verifies the password — it
does not call `AccessFailedAsync` on failure or `ResetAccessFailedCountAsync` on success (that
bookkeeping lives in `SignInManager`, which this codebase never uses). A repo-wide search confirms
`AccessFailedAsync` is called nowhere in `src/`. As a result `AccessFailedCount` never advances,
`IsLockedOutAsync` can never return true, and the lockout configured in `Program.cs` is dead code:
an attacker can submit unlimited password guesses against `/api/auth/login` for a given account,
limited only by nginx's per-IP rate limit (`web/nginx/default.conf:50-54`, 5 r/s), which does not
stop a slow or distributed guesser.

**Steps to Reproduce**
1. Register a user via `/api/auth/register`.
2. POST `/api/auth/login` with the wrong password more than 10 times (staying under 5 req/s, or
   from multiple source IPs).
3. Observe the account is never locked — `IsLockedOutAsync` continues to return `false` regardless
   of failed attempt count, because nothing ever calls `AccessFailedAsync`.

**Proposed Fix**
Either switch `AuthController.Login` to `SignInManager.CheckPasswordSignInAsync` (which manages the
lockout counters correctly), or explicitly call `userManager.AccessFailedAsync(user)` on a failed
password check and `userManager.ResetAccessFailedCountAsync(user)` on success.

**Notes**
None.

---

### MLM-002 — `Application:LoopToken` has no startup validation, silently weakening loop protection

- **ID:** MLM-002
- **Severity:** Medium
- **Status:** Open
- **Component:** API / Infrastructure (Mail)

**Description**
`ApplicationOptions.LoopToken` defaults to `string.Empty`
(`src/MailListManager.Core/Options/Options.cs:56`) and, unlike `Jwt:SigningKey`
(validated with a hard `throw` in `src/MailListManager.Api/Program.cs:43-47` if it is missing or
too short), nothing validates it at startup. `LoopGuard.StampOutbound` stamps every outbound message
with `_options.LoopToken` (`src/MailListManager.Infrastructure/Mail/LoopGuard.cs:117`), and
`LoopGuard.Evaluate` treats the loop header as present only when
`!string.IsNullOrEmpty(loopToken)` (`src/MailListManager.Infrastructure/Mail/LoopGuard.cs:41-48`).
If `LoopToken` is left blank (its default), the outbound header is effectively empty and the
strongest, most definitive loop-detection signal ("this message carries our own token") silently
never fires — the system falls back entirely to the weaker heuristic signals (Precedence,
Auto-Submitted, hop count, etc.). `deploy/docker-compose.yml` enforces a non-empty value via
`${LOOP_TOKEN:?set LOOP_TOKEN in .env}`, but any deployment path that doesn't go through that
compose file (e.g. App Service configured by hand, or `appsettings.json` alone) gets no warning at
all.

**Steps to Reproduce**
1. Run the API with `Application:LoopToken` unset (the shipped `appsettings.json` default).
2. Send a message that the list emits back to itself through an external forwarder that strips
   custom headers other than the loop header re-add — or simply note that `Evaluate`'s primary
   check is structurally unable to fire since the stamped value is empty.
3. Observe the API starts normally with no warning, unlike the analogous JWT signing-key case.

**Proposed Fix**
Validate `Application:LoopToken` at startup the same way `Jwt:SigningKey` is validated (e.g. require
a minimum length), or generate and persist a random one automatically if absent, logging a warning.

**Notes**
Related to the ICU/port startup-crash fixes in the recent git history — this codebase already has
precedent for "fail fast at startup rather than degrade silently" (see `Jwt:SigningKey` and
`DatabaseInitializer.ApplySchemaAsync`); this option was missed.

---

### MLM-003 — Oversized inbound messages keep their attachments despite the "rejected = no attachments" design intent

- **ID:** MLM-003
- **Severity:** Medium
- **Status:** Open
- **Component:** Infrastructure (Mail)

**Description**
`MailingListProcessor.ArchiveAsync` only persists attachment bytes when
`disposition is MessageDisposition.Held or MessageDisposition.Delivered`
(`src/MailListManager.Infrastructure/Mail/MailingListProcessor.cs:346-362`), with the comment
"Rejected mail is archived headers-and-body only; carrying attachments for discarded posts is a
storage and malware liability with no upside" (`MailingListProcessor.cs:344-345`). However, the
message-size check runs *after* the message has already been archived as `Held` (with attachments
saved and committed via `SaveChangesAsync` inside `ArchiveAsync`):
`var archived = await ArchiveAsync(list, message, messageId, MessageDisposition.Held, null, ct);`
(`MailingListProcessor.cs:272`), followed by the size check that flips the disposition to
`RejectedNonMember` (`MailingListProcessor.cs:274-282`). The attachments already written to the
database at line 272 are never removed, so a message that is ultimately rejected for being too
large still has its full attachment content stored — exactly the outcome the code's own comment
says it wants to avoid.

**Steps to Reproduce**
1. Configure a list with a small `MaxMessageBytes`.
2. Send a message with an attachment whose total size exceeds `MaxMessageBytes`.
3. After the poll, inspect `ArchivedMessage`/`MessageAttachment` rows for that message: the
   disposition is `RejectedNonMember`, but `MessageAttachments.Content` still contains the full
   attachment bytes.

**Proposed Fix**
Compute `SizeBytes` (or at least the attachment total) before calling `ArchiveAsync`, or check the
size threshold and skip attachment persistence when the message is going to be rejected for size,
consistent with how other rejection paths (loop, non-member) never store attachments at all.

**Notes**
None.

---

### MLM-004 — No automated tests anywhere in the repository

- **ID:** MLM-004
- **Severity:** Medium
- **Status:** Open
- **Component:** API / Core / Infrastructure / Worker / Web

**Description**
`MailListManager.slnx` lists four projects (`MailListManager.Api`, `.Core`, `.Infrastructure`,
`.Worker`) and none of them is a test project; a repo-wide search for `*Tests*` returns nothing.
`web/package.json` defines `dev`, `build`, and `preview` scripts only — no `test` script and no
test framework (Vitest, Jest, etc.) is present in `dependencies`/`devDependencies`. Security- and
correctness-sensitive logic — JWT issuance/refresh-token rotation (`TokenService`), the loop guard
(`LoopGuard`), access control (`ListAccessService`), and the mail-processing pipeline
(`MailingListProcessor`) — has no test coverage at all.

**Steps to Reproduce**
1. `find . -iname '*Tests*'` from the repo root returns no matches outside `.git`.
2. `MailListManager.slnx` contains no test project entries.
3. `web/package.json` has no `test` script.

**Proposed Fix**
Add an xUnit (or similar) test project referencing `MailListManager.Core`/`.Infrastructure` for
`LoopGuard`, `TokenService`, and `ListAccessService` at minimum, and a Vitest setup for the SPA's
`web/src/api` request/refresh logic.

**Notes**
None.

---

### MLM-005 — Long-lived refresh tokens are stored in `localStorage`

- **ID:** MLM-005
- **Severity:** Low
- **Status:** Open
- **Component:** Web

**Description**
`web/src/api/client.ts` persists both the access token and the 14-day refresh token
(`RefreshTokenDays` default in `src/MailListManager.Core/Options/Options.cs:16`) in
`localStorage` under the key `mlm.auth` (`web/src/api/client.ts:3`, `:34-41`). Any script able to
run in the page's origin — for example via a future dependency compromise or a CSP
misconfiguration — can read `localStorage` and exfiltrate a token valid for up to two weeks,
whereas an `HttpOnly` cookie would not be readable from script at all. The current strict CSP
(`web/nginx/security-headers.conf:11`, `script-src 'self'` with no `unsafe-inline`) substantially
reduces the practical risk today, which is why this is rated Low rather than higher.

**Steps to Reproduce**
1. Log in to the SPA.
2. Open the browser devtools console and run `localStorage.getItem('mlm.auth')`.
3. Observe the access token and 14-day refresh token in plaintext JSON.

**Proposed Fix**
Move the refresh token to an `HttpOnly`, `Secure`, `SameSite=Strict` cookie issued by the API, and
keep only the short-lived access token in memory/`localStorage`. This requires API changes (cookie
issuance/reading in `AuthController`/`TokenService`) as well as SPA changes, so treat as a larger
follow-up rather than a quick patch.

**Notes**
Mitigated in practice by the nginx CSP's `script-src 'self'` (no inline/eval), but defense-in-depth
still favors not storing long-lived credentials in script-accessible storage.
