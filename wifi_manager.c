//esp_netif_init() ---- initialize TCP/IP stack
//esp_event_loop_create_default()----wifi connection handle krne k liye event call kiya
//esp_netif_create_default_wifi_ap(); interface for server ---AP(access point)
//esp_netif_create_default_wifi_sta(); interface for client ---sta (station)
//(esp_wifi_init(&cfg)); wifi k driver ko initialize kiya, for tx / rx
//(esp_wifi_set_mode(WIFI_MODE_APSTA)); ap amd sta dono modes ko active kiya
//esp_wifi_set_mode(WIFI_MODE_STA);  Optional: if you want to turn off AP after connecting

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_http_server.h"
#include "cJSON.h"

#ifndef MIN //ifndef---if not defined
#define MIN(a,b) ((a) < (b) ? (a) : (b)) //agr a<b then return "a" else "b"
#endif

const char *TAG = "WiFi_Manager";
static bool is_connected=false;

const char *wifi_html_page =
"<!DOCTYPE html><html><body>"
"<h2>Custom Wi-Fi Manager</h2>"
"<button onclick='fetchScan()'>Scan Wi-Fi Networks</button><br><br>"
"<select id='ssidList'></select><br><br>"
"SSID: <input type='text' id='ssid'><br><br>"
"Password: <input type='password' id='password'>"
"<input type='checkbox' onclick='togglePassword()'> Show Password<br><br>"
"<button onclick='connectWifi()'>Connect to Wi-Fi</button><br><br>"
"<button onclick='checkStatus()'>Check Connection Status</button><br><br>"
"<p id='status'></p>"
"<script>"
"async function fetchScan(){"
"let res = await fetch('/scan');"
"let list = await res.json();"
"let sel = document.getElementById('ssidList');"
"sel.innerHTML='';"
"list.forEach(ssid => {"
"let opt = document.createElement('option'); opt.value = opt.text = ssid; sel.appendChild(opt); });}"
"function togglePassword(){"
"let pwd = document.getElementById('password');"
"pwd.type = pwd.type === 'password' ? 'text' : 'password';}"
"async function connectWifi(){"
"let ssid = document.getElementById('ssidList').value;"
"let pwd = document.getElementById('password').value;"
"let res = await fetch('/connect', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ssid:ssid,password:pwd})});"
"let txt = await res.text(); document.getElementById('status').innerText = txt;}"
"async function checkStatus(){"
"let res = await fetch('/status');"
"let txt = await res.text(); document.getElementById('status').innerText = txt;}"
"</script></body></html>";

//ye callback func hai qk is main event loop registered hain means jb jb event occur hoga system automatically isse call kry ga
void wifi_event_handler(void *arg, esp_event_base_t event_base,   
                               int32_t event_id, void *event_data) //callback func hai, event_id --- ye sta k connection and disconnect ko handle kry ga
                               //event_base--- wifi event and IP event, *event_data--- ye pointer hai related to extra data of events like wifi and IP
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA Started"); //wifi connect 
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "Disconnected from WiFi, reason: %d", disconn->reason);
        is_connected = false; //diconnect k liye false
    }else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data; //event_data ko cast kiya gaya hai to ip_event_got_ip_t* taake usme se IP nikala ja sake.
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip)); //IPSTR, IP2STR macros to print IP 
        is_connected = true; //ip mil jaye to connection true

        //  Log the connected SSID
        wifi_ap_record_t ap_info; //structure hai jo wifi AP ki details hold krta hai
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) //esp_wifi_sta_get_ap_info() Wi-Fi access point ke details laata hai, including SSID. 
        {
            ESP_LOGI(TAG, "Connected to SSID: %s", ap_info.ssid);//Agar function successful ho to, SSID log karte hain
        }
    }
}

//esp_err_t, ek 32bit type def hai jo simply error code ko represent krta hai, ESP_OK(0) means sb theek hai and ESP_FAIL means kuch ghlt
esp_err_t root_get_handler(httpd_req_t *req) //httpd_req_t, ek struct hai jo jo http request ki info hold krta hai
{
    httpd_resp_send(req, wifi_html_page, HTTPD_RESP_USE_STRLEN); //client ko rsponse send kry ga,webpage se connect kry ga
    return ESP_OK;
}

esp_err_t scan_handler(httpd_req_t *req) // Jab client /scan pe request bhejta hai, yeh function call hoga
{
    ESP_LOGI(TAG, "Starting WiFi Scan");

    wifi_scan_config_t scan_config = {
        .ssid = 0, //for all ssids
        .bssid = 0, //sary MAC scan hongy
        .channel = 0,
        .show_hidden = true // Hidden WiFi networks ko bhi dikhana
    };

    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) // WiFi scan start karne ki koshish kry ga
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    uint16_t ap_num = 0; // Total kitne WiFi access points milay, uske liye ek variable
    esp_wifi_scan_get_ap_num(&ap_num); // Jo WiFi milay unka count is variable mein bhar do
    wifi_ap_record_t ap_info[20]; // 20 WiFi records store karne ke liye ek array banaya
    memset(ap_info, 0, sizeof(ap_info)); //array k andr sb 0 krdiya for clean state

    if (esp_wifi_scan_get_ap_records(&ap_num, ap_info) != ESP_OK) //wifi k records lene ki try kry ga
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char resp[1024] = "["; // Response string banani start ki, ye ek JSON array banega
    for (int i = 0; i < ap_num; i++) {
        strcat(resp, "\""); //quote start
        strcat(resp, (char *)ap_info[i].ssid); //ssid add kro
        strcat(resp, "\"");//quote end
        if (i < ap_num - 1) strcat(resp, ","); //agr wifi credentials current waly nhi, koi new hain to "," daal kr add krdo in JSON array
    }
    strcat(resp, "]");//JSON string end

    httpd_resp_set_type(req, "application/json"); // ye line bataigi k client ko jo data milne wala hai wo JSON format main hai
    httpd_resp_send(req, resp, strlen(resp)); //// JSON string client ko bhej do
    return ESP_OK;
}

esp_err_t connect_post_handler(httpd_req_t *req) //ab wifi connect krne ka kaam start hoga
{
    char buf[256] = {0};//buffer 256 bytes ka jisme JSON data (SSID/password) store hoga
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1)); //Client se jo data aya hai wo buffer mein receive kar rahe hain (max 255 bytes)
    if (ret <= 0) //Agar data receive nahi hua ya error aaya, to server 500 error bhejta hai
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Parse JSON with cJSON
    cJSON *root = cJSON_Parse(buf); //json string ko examine krrha hai ta k ssid/password nikaal sky
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_sendstr(req, "JSON Parse Error");
        return ESP_FAIL;
    }

    const cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(root, "ssid"); //JSON object me se "ssid" aur "password" ke values nikaal rahe hain
    const cJSON *pass_json = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(pass_json)) //agr ssid and password sahi nhi to JSON string delete krdo or error send krdo
    {
        cJSON_Delete(root);
        httpd_resp_sendstr(req, "Invalid JSON data");
        return ESP_FAIL;
    }

    char ssid[33] = {0};
    char password[65] = {0};
    //strncpy(destination, source, num)---strncpy and limited size hota and safeb/c no overflow
    strncpy(ssid, ssid_json->valuestring, sizeof(ssid) - 1); //JSON se jo values mili unhe ssid aur password arrays mein copy kar rahe hain
    strncpy(password, pass_json->valuestring, sizeof(password) - 1);

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

    //wifi credentials NVS (flash memory) main save kro
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_creds", NVS_READWRITE, &nvs_handle);//nvs ko open kro usig "nvs_open()" func, NVS_READWRITE--- read write dono allow kro
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
    } else {
        nvs_set_str(nvs_handle, "ssid", ssid); //SSID aur password NVS mein save karo, changes commit karo, aur NVS band karo
        nvs_set_str(nvs_handle, "password", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }

    wifi_config_t wifi_config = {0}; //wifi_config struct ke andar SSID aur password assign kardo
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    esp_wifi_disconnect(); //Pehle se connected Wi-Fi ko disconnect kar raha hai
    esp_wifi_set_mode(WIFI_MODE_STA); //Wi-Fi mode ko station mode pe set kar rahe hain (jo kisi access point se connect hota hai)
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config); //onfiguration apply kar rahe hain aur Wi-Fi se connect kar rahe hain
    esp_wifi_connect();

    cJSON_Delete(root); //json root delete for memory clean up
    httpd_resp_sendstr(req, "Connecting to WiFi...");
    return ESP_OK;
}

esp_err_t status_get_handler(httpd_req_t *req) //simple GET handler function hai jo batata hai Wi-Fi connected hai ya nahi
{
    const char *msg = is_connected ? "Connected to Wi-Fi" : "Not Connected";
    //req---server response send kry ga,msg---- message jo client ko send krna hai, HTTPD_RESP_USE_STRLEN----string ki length 
    httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN); //mesage send to client
    return ESP_OK;
}

void start_webserver(void) //webserver start hoga
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); //HTTPD_DEFAULT_CONFIG()---ek macro hai jo tumhe ek pre-filled/default config struct de deta hai
    config.stack_size = 8192;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) // Agar server successfully start ho gaya to ye commands run hogi
    {
        httpd_register_uri_handler(server, &(httpd_uri_t){"/", HTTP_GET, root_get_handler}); // Jab user '/' (home) pe aaye, to root_get_handler() call hoga
        httpd_register_uri_handler(server, &(httpd_uri_t){"/scan", HTTP_GET, scan_handler}); //Jab user '/scan' request kare, to scan_handler() chalega (WiFi scan karega)
        httpd_register_uri_handler(server, &(httpd_uri_t){"/connect", HTTP_POST, connect_post_handler});
        httpd_register_uri_handler(server, &(httpd_uri_t){"/status", HTTP_GET, status_get_handler});
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}

void wifi_manager_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init()); //(Non-volatile storage) initialize kar rahe hain taake hum saved data access kar sakein
    ESP_ERROR_CHECK(esp_netif_init());//Network interface initialize kar rahe hain (WiFi, IP settings waghera)
    ESP_ERROR_CHECK(esp_event_loop_create_default());// Default event loop banate hain jahan WiFi/IP events handle honge

    esp_netif_create_default_wifi_sta();// WiFi ka default configuration wala structure bnaya
    esp_netif_create_default_wifi_ap();// esp32 ka wifi driver initialize krwaya

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    
    /*esp_event_handler_instance_register(
    esp_event_base_t event_base,      // e.g., WIFI_EVENT or IP_EVENT
    int32_t event_id,                 // Specific event ID or ESP_EVENT_ANY_ID
    esp_event_handler_t event_handler,// Pointer to your handler function
    void *event_handler_arg,          // Argument passed to handler (can be NULL)
    esp_event_handler_instance_t *instance // agr ksis event to thori dair bd unregister krna ho kisi specific condition pr
);
*/
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    nvs_handle_t nvs_handle;
    char ssid[33] = {0};
    char password[65] = {0};
    bool has_saved_creds = false;

    wifi_config_t wifi_config = {0};

    // NVS se SSID/PASSWORD nikal rahe hain
    if (nvs_open("wifi_creds", NVS_READONLY, &nvs_handle) == ESP_OK) {
        size_t ssid_len = sizeof(ssid);
        size_t pass_len = sizeof(password);
        if (nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(nvs_handle, "password", password, &pass_len) == ESP_OK) {
            has_saved_creds = true;
            ESP_LOGI(TAG, "Found saved credentials: SSID=%s", ssid);
            strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid)); //wifi_config struct ke andar SSID aur password assign kardo
            strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
        }
        nvs_close(nvs_handle);
    }

    if (has_saved_creds) {
        ESP_LOGI(TAG, "Trying to connect to saved Wi-Fi credentials...");

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); //station mode main gaya
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));// Station mode ka config set karo
        ESP_ERROR_CHECK(esp_wifi_start());
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_wifi_connect();

        //  Wait max 10 times for connection
        for (int retry = 0; retry < 10; retry++) {
            if (is_connected) {
                ESP_LOGI(TAG, " Connected to Wi-Fi using saved credentials.");
                return;  //  Connection done, exit function. AP mode nahi chalega
            }
            ESP_LOGI(TAG, "Waiting for WiFi connection... attempt %d", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        ESP_LOGW(TAG, " Failed to connect to saved Wi-Fi. Trying hardcoded...");
    }

    // Hardcoded WiFi try karo
    /*strncpy((char *)wifi_config.sta.ssid, "PC", sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, "12345678", sizeof(wifi_config.sta.password));
    ESP_LOGI(TAG, "Connecting to hardcoded Wi-Fi: SSID=PC");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_wifi_connect();

    for (int retry = 0; retry < 5; retry++) {
        if (is_connected) {
            ESP_LOGI(TAG, " Connected to hardcoded Wi-Fi (PC).");
            return;
        }
        ESP_LOGI(TAG, "Waiting for WiFi connection (PC)... attempt %d", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }*/

    ESP_LOGW(TAG, "Could not connect to any Wi-Fi. Enabling AP mode...");

    // Fallback: Start AP Mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "WiFi_Manager_AP",
            .ssid_len = 0,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK // wifi authentication key
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    start_webserver();
}

//URI --- ek UNIFORN RESOURCE IDENTIFIER hai, jo esp and webpage k b/w kis function ko access krna hai ye batata hai
/*JSON (JavaScript Object Notation):
Ek data format hai jo web aur devices (jaise ESP32) ke beech structured data ko send/receive karne ke liye use hota hai.
Example: {"ssid": "MyWiFi", "password": "12345678"}

cJSON:
Ek C language library hai jo JSON data ko parse (read), create, aur modify karne ke liye use hoti hai.
ESP-IDF mein web request/response handle karte waqt cJSON ka use hota hai.*/
    