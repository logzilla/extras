/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.ServiceProcess;
using System.Threading;

namespace SyslogAgent.Config {
    public class AgentService: ServiceModel {
        public string Status {
            get {
                try
                {
                    var controller = new ServiceController( serviceName );
                    return StatusName( controller.Status );
                }
                catch
                {
                    return "Error";
                }
            }
        }

        public void Restart(Action<string> showStatus ) {
            try {
                var controller = new ServiceController(serviceName);
                if (controller.Status != ServiceControllerStatus.Stopped &&
                    controller.Status != ServiceControllerStatus.StopPending) {
                    controller.Stop();
                }
                while (true) {
                    controller.Refresh();
                    var status = controller.Status;
                    showStatus(StatusName(status));
                    if (status == ServiceControllerStatus.Stopped) break;
                    Thread.Sleep(500);
                }
                controller.Start();

                while (true) {
                    controller.Refresh();
                    var status = controller.Status;
                    showStatus(StatusName(status));
                    if (status == ServiceControllerStatus.Running) break;
                    Thread.Sleep(500);
                }
            }
            catch (Exception e) {
                showStatus("in error: " + e.Message);
            }
        }

        static string StatusName(ServiceControllerStatus status) {
            switch (status) {
                case ServiceControllerStatus.ContinuePending:
                    return "pending continue";
                case ServiceControllerStatus.PausePending:
                    return "pending pause";
                case ServiceControllerStatus.Paused:
                    return "paused";
                case ServiceControllerStatus.Running:
                    return "running";
                case ServiceControllerStatus.StartPending:
                    return "pending start";
                case ServiceControllerStatus.StopPending:
                    return "pending stop";
                case ServiceControllerStatus.Stopped:
                    return "stopped";
                default:
                    return "unknown";
            }
        }

        const string serviceName = "LZ Syslog Agent";
    }
}
