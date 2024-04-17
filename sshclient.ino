#include <WiFi.h>
#include <M5Cardputer.h>
#include <WireGuard-ESP32.h>
#include <SD.h>
#include <FS.h>
#include "libssh_esp32.h"
#include <libssh/libssh.h>

#define BGCOLOR TFT_BLACK
#define FGCOLOR TFT_WHITE

// Saved credentials path
const char* SSH_CRED_FILE = "/sshclient/session.ssh";
const char* WIFI_CRED_FILE = "/sshclient/session.wifi";
const char* WG_CONFIG_FILE = "/sshclient/wg.conf";

// WireGuard variables
char private_key[45];
IPAddress local_ip;
char public_key[45];
char endpoint_address[16];
int endpoint_port = 31337;
static constexpr const uint32_t UPDATE_INTERVAL_MS = 5000;
static WireGuard wg;
bool useWireGuard = false;

// SSH variables
const char* ssid = "";
const char* password = "";
const char* ssh_host = "";
const char* ssh_user = "";
const char* ssh_password = "";

// M5Cardputer setup
M5Canvas canvas(&M5Cardputer.Display);
String commandBuffer = "";
int cursorY = 0;
const int lineHeight = 32;
unsigned long lastKeyPressMillis = 0;
const unsigned long debounceDelay = 150;

String readUserInput(bool isYesNoInput = false) {
    String input = "";
    bool inputComplete = false;

    while (!inputComplete) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
                for (auto i : status.word) {
                    input += i;
                    M5Cardputer.Display.print(i);
                }

                if (status.del && !input.isEmpty()) {
                    input.remove(input.length() - 1);
                    M5Cardputer.Display.setCursor(M5Cardputer.Display.getCursorX() - 6, M5Cardputer.Display.getCursorY());
                    M5Cardputer.Display.print(" "); // Print a space to erase the last character
                    M5Cardputer.Display.setCursor(M5Cardputer.Display.getCursorX() - 6, M5Cardputer.Display.getCursorY());
                }

                if (status.enter || (isYesNoInput && (input == "Y" || input == "y" || input == "N" || input == "n"))) {
                    inputComplete = true;
                }
            }
        }
    }
    return input;
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1); // Set text size

    M5Cardputer.Display.println("SSH Client v1.7 by SUB0PT1MAL");

    if (!SD.begin(SS)) {
        M5Cardputer.Display.println("Failed to mount SD card file system.");
        // You might want to add some error handling or retry logic here
        return;
    }
    // Prompt WiFi setup
    M5Cardputer.Display.print("\nUse saved WiFi credentials? (Y/N): ");
    String useWiFiCredentials = readUserInput(true);

    String ssid, password;
    bool wifiCredentialsLoadedFromFile = false;
    if (useWiFiCredentials == "Y" || useWiFiCredentials == "y") {
        if (loadWiFiCredentials(ssid, password)) {
            M5Cardputer.Display.println("\nWiFi credentials loaded.");
            wifiCredentialsLoadedFromFile = true;
        } else {
            M5Cardputer.Display.println("\nFailed to load WiFi credentials.");
            M5Cardputer.Display.print("\nSSID: ");
            ssid = readUserInput();
            M5Cardputer.Display.print("\nPassword: ");
            password = readUserInput();
        }
    } else {
        M5Cardputer.Display.print("\nSSID: ");
        ssid = readUserInput();
        M5Cardputer.Display.print("\nPassword: ");
        password = readUserInput();
    }

    if (!wifiCredentialsLoadedFromFile) { // Only prompt if credentials were not loaded from a file
        M5Cardputer.Display.print("\nSave WiFi credentials? (Y/N): ");
        String saveWiFiCredentials = readUserInput(true);
        if (saveWiFiCredentials == "Y" || saveWiFiCredentials == "y") {
            ::saveWiFiCredentials(ssid.c_str(), password.c_str());
        }
    }

    // Connect to WiFi
    //WiFi.begin(ssid, password);
    //while (WiFi.status() != WL_CONNECTED) {
    //    delay(500);
    //}

    // Connect to WiFi
    Serial.println("SSID contents:");
    Serial.println(ssid);

    Serial.println("SSID memory representation:");
    for (int i = 0; i < ssid.length(); i++) {
        Serial.print(static_cast<unsigned int>(ssid[i]), HEX);
        Serial.print(" ");
    }
    Serial.println();

    Serial.println("Password contents:");
    Serial.println(password);

    Serial.println("Password memory representation:");
    for (int i = 0; i < password.length(); i++) {
        Serial.print(static_cast<unsigned int>(password[i]), HEX);
        Serial.print(" ");
    }
    Serial.println();

    WiFi.begin(ssid.c_str(), password.c_str());

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    // Prompt for WireGuard setup
    M5Cardputer.Display.print("\nUse WireGuard VPN? (Y/N) WIP: ");
    String useWireGuardInput = readUserInput(true);
    while (useWireGuardInput != "Y" && useWireGuardInput != "y" && useWireGuardInput != "N" && useWireGuardInput != "n") {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
                for (auto i : status.word) {
                    useWireGuardInput += i;
                    M5Cardputer.Display.print(i);
                }
                if (status.enter) {
                    break;
                }
            }
        }
    }

    useWireGuard = (useWireGuardInput == "Y" || useWireGuardInput == "y");

    if (useWireGuard) {
        read_and_parse_file();
        wg_setup();
    }

    // Prompt SSH setup
    M5Cardputer.Display.print("\nUse saved SSH credentials? (Y/N): ");
    String useSSHCredentials = readUserInput(true);

    String ssh_host, ssh_user, ssh_password;
    bool sshCredentialsLoadedFromFile = false;
    if (useSSHCredentials == "Y" || useSSHCredentials == "y") {
        if (loadSSHCredentials(ssh_host, ssh_user, ssh_password)) {
            M5Cardputer.Display.println("\nSSH credentials loaded.");
            sshCredentialsLoadedFromFile = true;
        } else {
            M5Cardputer.Display.println("\nFailed to load SSH credentials.");
            M5Cardputer.Display.print("\nHost: ");
            ssh_host = readUserInput();
            M5Cardputer.Display.print("\nUsername: ");
            ssh_user = readUserInput();
            M5Cardputer.Display.print("\nPassword: ");
            ssh_password = readUserInput();
        }
    } else {
        M5Cardputer.Display.print("\nHost: ");
        ssh_host = readUserInput();
        M5Cardputer.Display.print("\nUsername: ");
        ssh_user = readUserInput();
        M5Cardputer.Display.print("\nPassword: ");
        ssh_password = readUserInput();
    }

    if (!sshCredentialsLoadedFromFile) { // Only prompt if credentials were not loaded from a file
        M5Cardputer.Display.print("\nSave SSH credentials? (Y/N): ");
        String saveSSHCredentials = readUserInput(true);
        if (saveSSHCredentials == "Y" || saveSSHCredentials == "y") {
            ::saveSSHCredentials(ssh_host.c_str(), ssh_user.c_str(), ssh_password.c_str());
        }
    }

    // Connect to SSH server
    TaskHandle_t sshTaskHandle = NULL;
    xTaskCreatePinnedToCore(sshTask, "SSH Task", 20000, NULL, 1, &sshTaskHandle, 1);
    if (sshTaskHandle == NULL) {
        Serial.println("Failed to create SSH Task");
    }

    // Initialize the cursor Y position
    cursorY = M5Cardputer.Display.getCursorY();
}

void loop() {
    M5Cardputer.update();

    if (useWireGuard) {
        wg_loop();
    }
}

void wg_loop() {

}

ssh_session connect_ssh(const char *host, const char *user, int verbosity) {
    ssh_session session = ssh_new();
    if (session == NULL) {
        Serial.println("\nFailed to create SSH session");
        return NULL;
    }

    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_USER, user);
    ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);

    if (ssh_connect(session) != SSH_OK) {
        Serial.print("\nError connecting to host: ");
        Serial.println(ssh_get_error(session));
        ssh_free(session);
        return NULL;
    }

    return session;
}

int authenticate_console(ssh_session session, const char *password) {
    int rc = ssh_userauth_password(session, NULL, password);
    if (rc != SSH_AUTH_SUCCESS) {
        Serial.print("\nError authenticating with password: ");
        Serial.println(ssh_get_error(session));
        return rc;
    }
    return SSH_OK;
}

void sshTask(void *pvParameters) {
    ssh_session my_ssh_session = connect_ssh(ssh_host, ssh_user, SSH_LOG_PROTOCOL);
    if (my_ssh_session == NULL) {
        M5Cardputer.Display.println("\nSSH Connection failed.");
        vTaskDelete(NULL);
        return;
    }

    M5Cardputer.Display.println("\nSSH Connection established.");
    if (authenticate_console(my_ssh_session, ssh_password) != SSH_OK) {
        M5Cardputer.Display.println("\nSSH Authentication failed.");
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        vTaskDelete(NULL);
        return;
    }

    M5Cardputer.Display.println("\nSSH Authentication succeeded.");

    // Open a new channel for the SSH session
    ssh_channel channel = ssh_channel_new(my_ssh_session);
    if (channel == NULL || ssh_channel_open_session(channel) != SSH_OK) {
        M5Cardputer.Display.println("\nSSH Channel open error.");
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        vTaskDelete(NULL);
        return;
    }

    // Request a pseudo-terminal
    if (ssh_channel_request_pty(channel) != SSH_OK) {
        M5Cardputer.Display.println("\nRequest PTY failed.");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        vTaskDelete(NULL);
        return;
    }

    // Start a shell session
    if (ssh_channel_request_shell(channel) != SSH_OK) {
        M5Cardputer.Display.println("\nRequest shell failed.");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        M5Cardputer.update();

        // Handle keyboard input with debounce
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                unsigned long currentMillis = millis();
                if (currentMillis - lastKeyPressMillis >= debounceDelay) {
                    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                    for (auto i : status.word) {
                        commandBuffer += i;
                        M5Cardputer.Display.print(i); // Display the character as it's typed
                        cursorY = M5Cardputer.Display.getCursorY(); // Update cursor Y position
                    }

                    if (status.del && commandBuffer.length() > 2) {
                        commandBuffer.remove(commandBuffer.length() - 1);
                        M5Cardputer.Display.setCursor(M5Cardputer.Display.getCursorX() - 6, M5Cardputer.Display.getCursorY());
                        M5Cardputer.Display.print(" "); // Print a space to erase the last character
                        M5Cardputer.Display.setCursor(M5Cardputer.Display.getCursorX() - 6, M5Cardputer.Display.getCursorY());
                        cursorY = M5Cardputer.Display.getCursorY(); // Update cursor Y position
                    }

                    if (status.enter) {
                        String message = commandBuffer.substring(2) + "\r\n"; // Use "\r\n" for newline
                        ssh_channel_write(channel, message.c_str(), message.length()); // Send message to SSH server

                        commandBuffer = "> ";
                        M5Cardputer.Display.print('\n'); // Move to the next line
                        cursorY = M5Cardputer.Display.getCursorY(); // Update cursor Y position
                    }

                    lastKeyPressMillis = currentMillis;
                }
            }
        }

        // Check if the cursor has reached the bottom of the display
        if (cursorY > M5Cardputer.Display.height() - lineHeight) {
            // Scroll the display up by one line
            M5Cardputer.Display.scroll(0, -lineHeight);

            // Reset the cursor to the new line position
            cursorY -= lineHeight;
            M5Cardputer.Display.setCursor(M5Cardputer.Display.getCursorX(), cursorY);
        }

        char buffer[1024];
        int nbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
        if (nbytes > 0) {
            for (int i = 0; i < nbytes; ++i) {
                if (buffer[i] == '\r') {
                    continue; // Handle carriage return
                }
                M5Cardputer.Display.write(buffer[i]);
                cursorY = M5Cardputer.Display.getCursorY(); // Update cursor Y position
            }
        }

        if (nbytes < 0 || ssh_channel_is_closed(channel)) {
            break;
        }
    }

    // Clean up
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(my_ssh_session);
    ssh_free(my_ssh_session);
    vTaskDelete(NULL);
}

void wg_setup()
{
    read_and_parse_file();

    Serial.println("Adjusting system time...");
    configTime(9 * 60 * 60, 0, "ntp.jst.mfeed.ad.jp", "ntp.nict.jp", "time.google.com");
    M5Cardputer.Display.clear();
    M5Cardputer.Display.setCursor(0, 0);

    Serial.println("Connected. Initializing WireGuard...");
    M5Cardputer.Display.println("Connecting to wireguard...");
    wg.begin(
        local_ip,
        private_key,
        endpoint_address,
        public_key,
        endpoint_port);
    Serial.println(local_ip);
    Serial.println(private_key);
    Serial.println(endpoint_address);
    Serial.println(public_key);
    Serial.println(endpoint_port);

    M5Cardputer.Display.clear();
    M5Cardputer.Display.setCursor(0, 0);

    M5Cardputer.Display.setTextColor(GREEN, BGCOLOR);
    M5Cardputer.Display.println("Connected!");
    M5Cardputer.Display.setTextColor(FGCOLOR, BGCOLOR);
    M5Cardputer.Display.print("IP on tunnel:");
    M5Cardputer.Display.setTextColor(WHITE, BGCOLOR);
    M5Cardputer.Display.println(local_ip);
    M5Cardputer.Display.setTextColor(FGCOLOR, BGCOLOR);
    Serial.println(local_ip);

}

void read_and_parse_file() {
  if (!SD.begin(SS)) {
    Serial.println("\nFailed to initialize SD card");
    return;
  }

  File file = SD.open(WG_CONFIG_FILE);
  if (!file) {
        M5Cardputer.Display.clear();
    M5Cardputer.Display.setCursor(0, 0);

    M5Cardputer.Display.setTextColor(RED, BGCOLOR);
    Serial.println("\nFailed to open file");
    M5Cardputer.Display.println("\nNo wg.conf file found");
    M5Cardputer.Display.setTextColor(FGCOLOR, BGCOLOR);
    delay(60000);
    return;
  }

  Serial.println("Readed config file!");

  Serial.println("Found file!");
  parse_config_file(file);
}

void parse_config_file(File configFile) {
  String line;

  while (configFile.available()) {
    line = configFile.readStringUntil('\n');
    Serial.println("==========PRINTING LINE");
    Serial.println(line);
    line.trim();

    if (line.startsWith("[Interface]") || line.isEmpty()) {
      // Skip [Interface] or empty lines
      continue;
    } else if (line.startsWith("PrivateKey")) {
      line.remove(0, line.indexOf('=') + 1);
      line.trim();
      Serial.println("Private Key: " + line);
      strncpy(private_key, line.c_str(), sizeof(private_key) - 1);
      private_key[sizeof(private_key) - 1] = '\0'; // Ensure null-terminated
    } else if (line.startsWith("Address")) {
      line.remove(0, line.indexOf('=') + 1);
      line.trim();
      Serial.println("Local IP: " + line);
      int slashIndex = line.indexOf('/');
      
      if (slashIndex != -1) {
        Serial.println("~~~~~~~~~~~~");
        Serial.println(line.substring(0, slashIndex));
        local_ip.fromString(line.substring(0, slashIndex));
      }

    } else if (line.startsWith("[Peer]")) {
      // Handle [Peer] section
    } else if (line.startsWith("PublicKey")) {
      line.remove(0, line.indexOf('=') + 1);
      line.trim();
      Serial.println("Public Key: " + line);
      strncpy(public_key, line.c_str(), sizeof(public_key) - 1);
      public_key[sizeof(public_key) - 1] = '\0'; // Ensure null-terminated
    } else if (line.startsWith("Endpoint")) {
      //Serial.println("~~~~~~~~~~~endpoint");
      //Serial.println(line);
      line.remove(0, line.indexOf('=') + 1);
      line.trim();
      int colonIndex = line.indexOf(':');

      if (colonIndex != -1) {
        //Serial.println("Endpoint Line: " + line);
        strncpy(endpoint_address, line.substring(0, colonIndex).c_str(), sizeof(endpoint_address) - 1);
        endpoint_address[sizeof(endpoint_address) - 1] = '\0'; // Ensure null-terminated
        Serial.println("Endpoint Address: " + String(endpoint_address));
        endpoint_port = line.substring(colonIndex + 1).toInt();
        Serial.println("Endpoint Port: " + String(endpoint_port));
      }
    }
  }

  Serial.println("Closing file!");
  configFile.close();
}

void saveWiFiCredentials(const char* ssid, const char* password) {
    if (!SD.begin(SS)) {
        M5Cardputer.Display.println("\nFailed to initialize SD card.");
        return;
    }
    Serial.println("SD card initialized successfully.");

    File file = SD.open(WIFI_CRED_FILE, FILE_WRITE);
    if (!file) {
        M5Cardputer.Display.println("\nFailed to open file for writing WiFi credentials.");
        return;
    }
    Serial.println("File opened successfully for writing WiFi credentials.");

    file.println(ssid);
    file.print(password);
    file.close();
    M5Cardputer.Display.println("\nWiFi credentials saved.");
}

bool loadWiFiCredentials(String& ssid, String& password) {
    File file = SD.open(WIFI_CRED_FILE, FILE_READ);
    if (!file) {
        Serial.println("Failed to open WiFi credentials file.");
        return false;
    }
    Serial.println("WiFi credentials file opened successfully.");

    ssid = file.readStringUntil('\n');
    password = file.readStringUntil('\n');
    ssid.trim();
    password.trim();
    file.close();

    // Create new char arrays or std::string objects to store the loaded credentials
    static char ssid_buf[128];
    static char password_buf[128];

    strncpy(ssid_buf, ssid.c_str(), sizeof(ssid_buf) - 1);
    ssid_buf[sizeof(ssid_buf) - 1] = '\0';
    strncpy(password_buf, password.c_str(), sizeof(password_buf) - 1);
    password_buf[sizeof(password_buf) - 1] = '\0';

    // Update the global const char* variables with the loaded credentials
    ssid = ssid_buf;
    password = password_buf;

    Serial.printf("SSID: %s\nPassword: %s\n", ssid, password);
    return true;
}

void saveSSHCredentials(const char* host, const char* user, const char* password) {
    File file = SD.open(SSH_CRED_FILE, FILE_WRITE);
    if (!SD.begin(SS)) {
        M5Cardputer.Display.println("\nFailed to initialize SD card.");
        return;
    }
    if (file) {
        file.println(host);
        file.println(user);
        file.print(password);
        file.close();
        M5Cardputer.Display.println("\nSSH credentials saved.");
    } else {
        M5Cardputer.Display.println("\nFailed to save SSH credentials.");
    }
}

bool loadSSHCredentials(String& host, String& user, String& password) {
    File file = SD.open(SSH_CRED_FILE, FILE_READ);
    if (!file) {
        Serial.println("Failed to open SSH credentials file.");
        return false;
    }
    Serial.println("SSH credentials file opened successfully.");

    host = file.readStringUntil('\n');
    user = file.readStringUntil('\n');
    password = file.readStringUntil('\n');
    host.trim();
    user.trim();
    password.trim();
    file.close();

    // Create new char arrays or std::string objects to store the loaded credentials
    static char ssh_host_buf[128];
    static char ssh_user_buf[128];
    static char ssh_password_buf[128];

    strncpy(ssh_host_buf, host.c_str(), sizeof(ssh_host_buf) - 1);
    ssh_host_buf[sizeof(ssh_host_buf) - 1] = '\0';
    strncpy(ssh_user_buf, user.c_str(), sizeof(ssh_user_buf) - 1);
    ssh_user_buf[sizeof(ssh_user_buf) - 1] = '\0';
    strncpy(ssh_password_buf, password.c_str(), sizeof(ssh_password_buf) - 1);
    ssh_password_buf[sizeof(ssh_password_buf) - 1] = '\0';

    // Assign the loaded credentials to the global const char* variables
    ssh_host = ssh_host_buf;
    ssh_user = ssh_user_buf;
    ssh_password = ssh_password_buf;

    return true;
}