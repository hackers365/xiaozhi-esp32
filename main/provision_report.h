#ifndef PROVISION_REPORT_H
#define PROVISION_REPORT_H

#include <string>

namespace ProvisionReport {

void ReportWifiProvisionedAsync(const std::string& ssid, const std::string& ip);

}  // namespace ProvisionReport

#endif  // PROVISION_REPORT_H
