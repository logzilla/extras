/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading.Tasks;
using System.Net.Security;

namespace SyslogAgent.Config
{

    using System;
    using System.Net.Http;
    using System.Net.Security;
    using System.Security.Authentication;
    using System.Security.Cryptography.X509Certificates;
    using System.Threading.Tasks;

    public class CertificateChecker
    {
        private X509Certificate2 clientCertificate;

        public CertificateChecker( string pfxFilePath, string pfxPassword )
        {
            // Load the certificate from the PFX file
            clientCertificate = new X509Certificate2( pfxFilePath, pfxPassword );
        }

        public async Task<bool> CheckRemoteCertificateAsync( string remoteServerUrl )
        {
            // Set up the HTTP client handler
            var handler = new HttpClientHandler();

            // Assign the server certificate validation callback
            handler.ServerCertificateCustomValidationCallback = ServerCertificateValidationCallback;

            // Create an HTTP client with the handler
            using( var client = new HttpClient( handler ) )
            {
                try
                {
                    client.Timeout = TimeSpan.FromSeconds( 30 ); // Set timeout to 30 seconds
                    // Make a GET request to the remote server
                    var response = await client.GetAsync( remoteServerUrl ).ConfigureAwait( false );


                    // Check the status code or other properties of the response if needed
                    // For now, just returning true to indicate that the certificate matched
                    // and the request was successful
                    return response.IsSuccessStatusCode;
                }
                catch( HttpRequestException ex)
                {
                    // Handle exception if the request failed, e.g., due to certificate mismatch
                    // Console.WriteLine( $"Error during request: {aex.Message}" );
                    return false;
                }
            }
        }

        public bool CheckRemoteCertificateSynchronous( string remoteServerUrl )
        {
            try
            {
                // Call the asynchronous method and block until it completes
                var task = CheckRemoteCertificateAsync( remoteServerUrl );
                return task.GetAwaiter().GetResult(); // This will block until the task is complete
            }
            catch( AggregateException ae )
            {
                // Handle exceptions (if any) here
                foreach( var e in ae.Flatten().InnerExceptions )
                {
                    Console.WriteLine( $"Error during request: {e.Message}" );
                }
                return false;
            }
        }


        private bool ServerCertificateValidationCallback( HttpRequestMessage httpRequestMessage, X509Certificate2 cert, X509Chain cetChain, SslPolicyErrors policyErrors )
        {
            // Compare the thumbprint of the remote server's certificate with the one from the PFX file
            return cert.Thumbprint == clientCertificate.Thumbprint;
        }
    }



}