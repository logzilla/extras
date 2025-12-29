namespace SyslogAgent.Config
{
    public class LogLevelChoice
    {
        /// <summary>
        /// The name displayed in the dropdown (e.g., "DEBUG2", "INFO").
        /// </summary>
        public string DisplayName { get; set; }

        /// <summary>
        /// The integer value to be stored in Configuration.DebugLevel.
        /// This value, when the 9-X transformation is applied, will result in the correct registry value for the agent.
        /// </summary>
        public int ConfigValue { get; set; }
    }
}
