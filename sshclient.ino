#include <WiFi.h>
#include <M5Cardputer.h>
#include <WireGuard-ESP32.h>
#include <SD.h>
#include <FS.h>
#include "libssh_esp32.h"
#include <libssh/libssh.h>

#define BGCOLOR TFT_BLACK
#define FGCOLOR TFT_WHITE

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

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1); // Set text size

    // Prompt WiFi setup
    M5Cardputer.Display.print("WiFi setup");

    M5Cardputer.Display.print("\nSSID: ");
    readInputFromKeyboard(ssid);

    M5Cardputer.Display.print("\nPassword: ");
    readInputFromKeyboard(password);

    Serial.println(ssid);
    Serial.println(password); 

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    // Prompt for WireGuard setup
    M5Cardputer.Display.print("\nUse WireGuard VPN? (Y/N) WIP: ");
    String useWireGuardInput = "";
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
    M5Cardputer.Display.print("\nSSH setup ");

    M5Cardputer.Display.print("\nHost: ");
    readInputFromKeyboard(ssh_host);

    M5Cardputer.Display.print("\nUsername: ");
    readInputFromKeyboard(ssh_user);

    M5Cardputer.Display.print("\nPassword: ");
    readInputFromKeyboard(ssh_password);

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

void readInputFromKeyboard(const char*& inputVariable) {
    commandBuffer = "";
    bool inputComplete = false;

    while (!inputComplete) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                for (auto i : status.word) {
                    commandBuffer += i;
                    M5Cardputer.Display.print(i); // Display the character as it's typed
                    cursorY = M5Cardputer.Display.getCursorY(); // Update cursor Y position
                }

                if (status.del && commandBuffer.length() > 0) {
                    commandBuffer.remove(commandBuffer.length() - 1);
                    M5Cardputer.Display.setCursor(M5Cardputer.Display.getCursorX() - 6, M5Cardputer.Display.getCursorY());
                    M5Cardputer.Display.print(" "); // Print a space to erase the last character
                    M5Cardputer.Display.setCursor(M5Cardputer.Display.getCursorX() - 6, M5Cardputer.Display.getCursorY());
                    cursorY = M5Cardputer.Display.getCursorY(); // Update cursor Y position
                }

                if (status.enter) {
                    inputComplete = true;
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
    }

    char* inputBuffer = new char[commandBuffer.length() + 1];
    strcpy(inputBuffer, commandBuffer.c_str());
    inputBuffer[commandBuffer.length()] = '\0'; // Add null terminator
    inputVariable = inputBuffer;
    commandBuffer = "";
}

ssh_session connect_ssh(const char *host, const char *user, int verbosity) {
    ssh_session session = ssh_new();
    if (session == NULL) {
        Serial.println("Failed to create SSH session");
        return NULL;
    }

    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_USER, user);
    ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);

    if (ssh_connect(session) != SSH_OK) {
        Serial.print("Error connecting to host: ");
        Serial.println(ssh_get_error(session));
        ssh_free(session);
        return NULL;
    }

    return session;
}

int authenticate_console(ssh_session session, const char *password) {
    int rc = ssh_userauth_password(session, NULL, password);
    if (rc != SSH_AUTH_SUCCESS) {
        Serial.print("Error authenticating with password: ");
        Serial.println(ssh_get_error(session));
        return rc;
    }
    return SSH_OK;
}

void sshTask(void *pvParameters) {
    ssh_session my_ssh_session = connect_ssh(ssh_host, ssh_user, SSH_LOG_PROTOCOL);
    if (my_ssh_session == NULL) {
        M5Cardputer.Display.println("SSH Connection failed.");
        vTaskDelete(NULL);
        return;
    }

    M5Cardputer.Display.println("SSH Connection established.");
    if (authenticate_console(my_ssh_session, ssh_password) != SSH_OK) {
        M5Cardputer.Display.println("SSH Authentication failed.");
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        vTaskDelete(NULL);
        return;
    }

    M5Cardputer.Display.println("SSH Authentication succeeded.");

    // Open a new channel for the SSH session
    ssh_channel channel = ssh_channel_new(my_ssh_session);
    if (channel == NULL || ssh_channel_open_session(channel) != SSH_OK) {
        M5Cardputer.Display.println("SSH Channel open error.");
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        vTaskDelete(NULL);
        return;
    }

    // Request a pseudo-terminal
    if (ssh_channel_request_pty(channel) != SSH_OK) {
        M5Cardputer.Display.println("Request PTY failed.");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        vTaskDelete(NULL);
        return;
    }

    // Start a shell session
    if (ssh_channel_request_shell(channel) != SSH_OK) {
        M5Cardputer.Display.println("Request shell failed.");
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
    M5Cardputer.Display.println("Connecting to\nwireguard...");
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
    Serial.println("Failed to initialize SD card");
    return;
  }

  File file = SD.open("/wg.conf");
  if (!file) {
        M5Cardputer.Display.clear();
    M5Cardputer.Display.setCursor(0, 0);

    M5Cardputer.Display.setTextColor(RED, BGCOLOR);
    Serial.println("Failed to open file");
    M5Cardputer.Display.println("No wg.conf file\nfound on\nthe SD");
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
