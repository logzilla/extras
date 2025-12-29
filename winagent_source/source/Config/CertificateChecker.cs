using System;
using System.Linq;
using System.Net.Http;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Threading.Tasks;

public class CertificateChecker
{
    private readonly string _pfxPath;
    private readonly string _pfxPassword;
    private X509Certificate2 _localCertificate;

    public CertificateChecker(string pfxPath, string pfxPassword)
    {
        _pfxPath = pfxPath;
        _pfxPassword = pfxPassword;
        LoadLocalCertificate();
    }

    private void LoadLocalCertificate()
    {
        try
        {
            _localCertificate = new X509Certificate2(_pfxPath, _pfxPassword);
        }
        catch (Exception ex)
        {
            throw new Exception($"Failed to load PFX certificate from {_pfxPath}: {ex.Message}");
        }
    }

    public bool CheckRemoteCertificateSynchronous(string url)
    {
        try
        {
            using (var handler = new HttpClientHandler())
            {
                handler.ServerCertificateCustomValidationCallback = ValidateServerCertificate;
                using (var client = new HttpClient(handler))
                {
                    // Make a HEAD request to minimize data transfer
                    var response = client.SendAsync(new HttpRequestMessage(HttpMethod.Head, url))
                        .GetAwaiter().GetResult();
                    return response.IsSuccessStatusCode;
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Certificate validation failed: {ex.Message}");
            return false;
        }
    }

    private bool ValidateServerCertificate(
        HttpRequestMessage request,
        X509Certificate2 serverCertificate,
        X509Chain chain,
        SslPolicyErrors sslPolicyErrors)
    {
        try
        {
            // For self-signed certificates, we'll ignore standard SSL policy errors
            // Instead, we'll compare the certificates directly

            // Convert the server certificate if needed
            var serverCert = serverCertificate as X509Certificate2 ??
                new X509Certificate2(serverCertificate);

            // Compare relevant certificate properties
            bool thumbprintMatch = _localCertificate.Thumbprint.Equals(
                serverCert.Thumbprint, StringComparison.OrdinalIgnoreCase);
            bool publicKeyMatch = _localCertificate.GetPublicKey().SequenceEqual(
                serverCert.GetPublicKey());
            bool subjectMatch = _localCertificate.Subject.Equals(
                serverCert.Subject, StringComparison.OrdinalIgnoreCase);

            // Log validation details (optional)
            Console.WriteLine($"Certificate Validation Details:");
            Console.WriteLine($"Thumbprint Match: {thumbprintMatch}");
            Console.WriteLine($"Public Key Match: {publicKeyMatch}");
            Console.WriteLine($"Subject Match: {subjectMatch}");

            // Return true if all critical elements match
            return thumbprintMatch && publicKeyMatch;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error during certificate validation: {ex.Message}");
            return false;
        }
    }

    public async Task<bool> CheckRemoteCertificateAsync(string url)
    {
        try
        {
            using (var handler = new HttpClientHandler())
            {
                handler.ServerCertificateCustomValidationCallback = ValidateServerCertificate;
                using (var client = new HttpClient(handler))
                {
                    var response = await client.SendAsync(
                        new HttpRequestMessage(HttpMethod.Head, url));
                    return response.IsSuccessStatusCode;
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Certificate validation failed: {ex.Message}");
            return false;
        }
    }

    // Helper method to dispose of resources
    public void Dispose()
    {
        _localCertificate?.Dispose();
    }
}

