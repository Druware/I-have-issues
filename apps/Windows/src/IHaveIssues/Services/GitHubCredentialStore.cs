using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using IssuesKit;

namespace IHaveIssues.Services;

/// <summary>
/// The GitHub personal access token, stored in Windows Credential Manager.
/// </summary>
/// <remarks>
/// Owner and repository are <b>not</b> here: they are per-project coordinates and live in the
/// document (<c>Model.Integrations.GitHub</c>), edited in Project Settings. The token stays in
/// Credential Manager and is never written into an <c>.issues</c> file, because those files are
/// committed to project repositories.
/// <para>
/// One item per repository. A token issued for one repository must never be sent to another, so
/// every stored item is keyed by the target returned from <see cref="Target"/> and a document only
/// ever reads back the token saved against its own coordinates.
/// </para>
/// <para>
/// This is the Windows counterpart of the Apple build's Keychain store. Credentials are written
/// with <c>CRED_PERSIST_LOCAL_MACHINE</c>, which keeps them on this machine and out of a roaming
/// profile — the same intent as the Keychain's <c>ThisDeviceOnly</c> accessibility.
/// </para>
/// </remarks>
public static class GitHubCredentialStore
{
    private const string ServicePrefix = "IHaveIssues-GitHubToken";

    /// <summary>
    /// The Credential Manager target that scopes a token to one repository:
    /// <c>IHaveIssues-GitHubToken:&lt;owner&gt;/&lt;repository&gt;</c>, trimmed and lowercased so
    /// the same repository always resolves to the same item.
    /// </summary>
    /// <returns>
    /// <c>null</c> when the integration is absent or either coordinate is blank — there is no
    /// repository to scope to, so there is nothing to save, load, or delete.
    /// </returns>
    public static string? Target(GitHubIntegration? integration)
    {
        if (integration is null)
        {
            return null;
        }

        var owner = integration.Owner.Trim().ToLowerInvariant();
        var repository = integration.Repository.Trim().ToLowerInvariant();
        return owner.Length == 0 || repository.Length == 0 ? null : $"{ServicePrefix}:{owner}/{repository}";
    }

    /// <summary>Stores a token, replacing any token already held for the same repository.</summary>
    /// <exception cref="IOException">Credential Manager rejected the write.</exception>
    public static void Save(string token, string target)
    {
        var blob = Encoding.Unicode.GetBytes(token);
        var blobHandle = Marshal.AllocHGlobal(blob.Length);
        try
        {
            Marshal.Copy(blob, 0, blobHandle, blob.Length);
            var credential = new Credential
            {
                Type = CredTypeGeneric,
                TargetName = target,
                CredentialBlob = blobHandle,
                CredentialBlobSize = blob.Length,
                Persist = CredPersistLocalMachine,
                UserName = ServicePrefix
            };

            if (!CredWriteW(ref credential, 0))
            {
                throw new IOException(
                    "The GitHub token could not be saved to Windows Credential Manager.",
                    Marshal.GetLastPInvokeError());
            }
        }
        finally
        {
            Marshal.FreeHGlobal(blobHandle);
        }
    }

    /// <summary>The token stored for <paramref name="target"/>, or <c>null</c> when there is none.</summary>
    public static string? Load(string target)
    {
        if (!CredReadW(target, CredTypeGeneric, 0, out var handle))
        {
            return null;
        }

        try
        {
            var credential = Marshal.PtrToStructure<Credential>(handle);
            if (credential.CredentialBlob == IntPtr.Zero || credential.CredentialBlobSize == 0)
            {
                return null;
            }

            // The blob is raw bytes, not a string: read the exact byte count that was written.
            return Marshal.PtrToStringUni(credential.CredentialBlob, credential.CredentialBlobSize / 2);
        }
        finally
        {
            CredFree(handle);
        }
    }

    /// <summary>
    /// Whether a token is stored for <paramref name="target"/>, without reading it back.
    /// </summary>
    /// <remarks>
    /// The value is freed without being converted deliberately: callers that only need to know an
    /// item exists — the sync window, so it can offer to replace or remove one — must never hold
    /// the secret to answer that question.
    /// </remarks>
    public static bool HasToken(string target)
    {
        if (!CredReadW(target, CredTypeGeneric, 0, out var handle))
        {
            return false;
        }

        CredFree(handle);
        return true;
    }

    /// <summary>Removes the stored token. Succeeds silently when there is nothing to remove.</summary>
    public static void Delete(string target) => CredDeleteW(target, CredTypeGeneric, 0);

    // MARK: - Interop

    private const int CredTypeGeneric = 1;
    private const int CredPersistLocalMachine = 2;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct Credential
    {
        public int Flags;
        public int Type;
        public string TargetName;
        public string? Comment;
        public long LastWritten;
        public int CredentialBlobSize;
        public IntPtr CredentialBlob;
        public int Persist;
        public int AttributeCount;
        public IntPtr Attributes;
        public string? TargetAlias;
        public string UserName;
    }

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CredWriteW(ref Credential credential, int flags);

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CredReadW(string target, int type, int reservedFlag, out IntPtr credential);

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CredDeleteW(string target, int type, int flags);

    [DllImport("advapi32.dll")]
    private static extern void CredFree(IntPtr buffer);
}
