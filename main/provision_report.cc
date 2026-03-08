#include "provision_report.h"

#include "board.h"
#include "system_info.h"

#include <cJSON.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <utility>

namespace {

constexpr const char* TAG = "ProvisionReport";
constexpr const char* kProvisionReportUrl =
    "http://10.0.0.157:8090/api/public/device/provision-report";

struct ReportContext {
    std::string ssid;
    std::string ip;
};

std::string GetProvisionDeviceId() {
    std::string device_id = SystemInfo::GetMacAddress();
    std::transform(device_id.begin(), device_id.end(), device_id.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return device_id;
}

void SendProvisionReport(const ReportContext& context) {
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to create provision report JSON object");
        return;
    }

    std::string device_id = GetProvisionDeviceId();
    cJSON_AddStringToObject(root, "device_id", device_id.c_str());
    if (!context.ssid.empty()) {
        cJSON_AddStringToObject(root, "ssid", context.ssid.c_str());
    }
    if (!context.ip.empty()) {
        cJSON_AddStringToObject(root, "ip", context.ip.c_str());
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == nullptr) {
        ESP_LOGE(TAG, "Failed to serialize provision report JSON");
        return;
    }

    std::string body(json);
    cJSON_free(json);

    auto http = Board::GetInstance().GetNetwork()->CreateHttp(10);
    http->SetHeader("Content-Type", "application/json");
    http->SetContent(std::move(body));

    if (!http->Open("POST", kProvisionReportUrl)) {
        ESP_LOGE(TAG, "Failed to open provision report request, code=0x%x", http->GetLastError());
        return;
    }

    const int status_code = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    if (status_code != 200) {
        ESP_LOGW(TAG, "Provision report failed, status=%d, body=%s", status_code, response.c_str());
        return;
    }

    ESP_LOGI(TAG, "Provision reported successfully, response=%s", response.c_str());
}

}  // namespace

namespace ProvisionReport {

void ReportWifiProvisionedAsync(const std::string& ssid, const std::string& ip) {
    auto* context = new ReportContext{ssid, ip};
    BaseType_t task_created = xTaskCreate(
        [](void* arg) {
            std::unique_ptr<ReportContext> context(static_cast<ReportContext*>(arg));
            SendProvisionReport(*context);
            vTaskDelete(nullptr);
        },
        "provision_report", 6144, context, 4, nullptr);

    if (task_created != pdPASS) {
        delete context;
        ESP_LOGE(TAG, "Failed to create provision report task");
    }
}

}  // namespace ProvisionReport
