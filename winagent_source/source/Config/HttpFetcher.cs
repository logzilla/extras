/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Net.Http;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Threading.Tasks;

namespace SyslogAgent.Config
{
    public class HttpFetcher : IDisposable
    {
        private readonly HttpClient _client;
        private readonly X509Certificate2 _certificate;

        public HttpFetcher(string pfxPath = null, string pfxPassword = null)
        {
            if (pfxPath != null)
            {
                try
                {
                    // Add proper flags for certificate handling
                    _certificate = new X509Certificate2(
                        pfxPath,
                        pfxPassword,
                        X509KeyStorageFlags.MachineKeySet |
                        X509KeyStorageFlags.EphemeralKeySet
                    );

                    var handler = new HttpClientHandler
                    {
                        ServerCertificateCustomValidationCallback = ValidateServerCertificate
                    };
                    _client = new HttpClient(handler);
                }
                catch (Exception ex)
                {
                    _certificate?.Dispose();
                    throw new Exception($"Failed to initialize HTTPS client: {ex.Message}");
                }
            }
            else
            {
                _client = new HttpClient();
            }

            _client.Timeout = TimeSpan.FromSeconds(30);
        }

        private bool ValidateServerCertificate(
            HttpRequestMessage request,
            X509Certificate2 serverCertificate,
            X509Chain chain,
            SslPolicyErrors sslPolicyErrors)
        {
            if (_certificate == null)
                return true; // No certificate verification requested

            try
            {
                var serverCert = serverCertificate as X509Certificate2 ??
                    new X509Certificate2(serverCertificate);

                // Compare certificate properties
                return _certificate.Thumbprint.Equals(
                    serverCert.Thumbprint, StringComparison.OrdinalIgnoreCase);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Certificate validation error: {ex.Message}");
                return false;
            }
        }

        public async Task<(bool success, string response, string error)> GetAsync(
            string url, string apiKey, bool validateSsl = true)
        {
            try
            {
                var request = new HttpRequestMessage(HttpMethod.Get, url);

                // Set up headers
                request.Headers.Accept.Add(
                    new System.Net.Http.Headers.MediaTypeWithQualityHeaderValue("application/json"));

                if (!string.IsNullOrEmpty(apiKey))
                {
                    request.Headers.Add("Authorization", $"token {apiKey.Trim()}");
                }

                // Send request
                using (var response = await _client.SendAsync(request))
                {
                    string content = await response.Content.ReadAsStringAsync();

                    if (response.IsSuccessStatusCode)
                    {
                        return (true, content, null);
                    }

                    return (false, null,
                        $"API request failed: {response.StatusCode} - {response.ReasonPhrase}");
                }
            }
            catch (HttpRequestException ex)
            {
                return (false, null, $"HTTP Request failed: {ex.Message}");
            }
            catch (TaskCanceledException)
            {
                return (false, null, "Request timed out");
            }
            catch (Exception ex)
            {
                return (false, null, $"Unexpected error: {ex.Message}");
            }
        }

        public (bool success, string response, string error) GetSynchronous(
            string url, string apiKey, bool validateSsl = true)
        {
            try
            {
                return GetAsync(url, apiKey, validateSsl).GetAwaiter().GetResult();
            }
            catch (Exception ex)
            {
                return (false, null, $"Request failed: {ex.Message}");
            }
        }

        public void Dispose()
        {
            _client?.Dispose();
            _certificate?.Dispose();
        }
    }

}

