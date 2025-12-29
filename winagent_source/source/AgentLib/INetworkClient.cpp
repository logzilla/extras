#include "pch.h"
// #include "stdafx.h" // Removed, pch.h is used
#include "INetworkClient.h"

namespace Syslog_agent {

	const INetworkClient::RESULT_TYPE INetworkClient::RESULT_SUCCESS(NetworkErrorCode::Success, "Success");

} // namespace Syslog_agent
