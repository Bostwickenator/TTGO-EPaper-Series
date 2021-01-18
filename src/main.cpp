// include library, include base class, make path known
#include <GxEPD.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBoldOblique9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoOblique9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBoldOblique9pt7b.h>
#include <Fonts/FreeSansOblique9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerifBold9pt7b.h>
#include <Fonts/FreeSerifBoldItalic9pt7b.h>
#include <Fonts/FreeSerifItalic9pt7b.h>
#include <Fonts/Picopixel.h>
//#define DEFALUT_FONT  FreeMono9pt7b
// #define DEFALUT_FONT  FreeMonoBoldOblique9pt7b
// #define DEFALUT_FONT FreeMonoBold9pt7b
// #define DEFALUT_FONT FreeMonoOblique9pt7b
//#define DEFALUT_FONT FreeSans9pt7b
// #define DEFALUT_FONT FreeSansBold9pt7b
// #define DEFALUT_FONT FreeSansBoldOblique9pt7b
// #define DEFALUT_FONT FreeSansOblique9pt7b
// #define DEFALUT_FONT FreeSerif9pt7b
#define DEFALUT_FONT FreeSerifBold9pt7b
// #define DEFALUT_FONT FreeSerifBoldItalic9pt7b
// #define DEFALUT_FONT FreeSerifItalic9pt7b

const GFXfont *fonts[] = {
    &FreeMono9pt7b,
    &FreeMonoBoldOblique9pt7b,
    &FreeMonoBold9pt7b,
    &FreeMonoOblique9pt7b,
    &FreeSans9pt7b,
    &FreeSansBold9pt7b,
    &FreeSansBoldOblique9pt7b,
    &FreeSansOblique9pt7b,
    &FreeSerif9pt7b,
    &FreeSerifBold9pt7b,
    &FreeSerifBoldItalic9pt7b,
    &FreeSerifItalic9pt7b};

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include "SD.h"
#include "SPI.h"
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include "Esp.h"
#include "board_def.h"
#include <Button2.h>
#include <math.h>

#define FILESYSTEM SPIFFS

#define USE_AP_MODE

/*100 * 100 bmp fromat*/
//https://www.onlineconverter.com/jpg-to-bmp
#define BADGE_CONFIG_FILE_NAME "/badge.data"
#define DEFALUT_AVATAR_BMP "/avatar.bmp"
#define DEFALUT_QR_CODE_BMP "/qr.bmp"
#define WIFI_SSID "Put your wifi ssid"
#define WIFI_PASSWORD "Put your wifi password"
#define CHANNEL_0 0
#define IP5306_ADDR 0X75
#define IP5306_REG_SYS_CTL0 0x00

class BookInfo
{
public:
    int position;
    int nextPagePosition;
};

void showMianPage(void);
void showQrPage(void);
void displayInit(void);
void drawBitmap(const char *filename, int16_t x, int16_t y, bool with_color);
void renderPage();
void copyFile(String filepath);
BookInfo retriveBookInformation();

typedef struct
{
    char name[32];
    char link[64];
    char tel[64];
    char company[64];
    char email[64];
    char address[128];
} Badge_Info_t;

typedef enum
{
    RIGHT_ALIGNMENT = 0,
    LEFT_ALIGNMENT,
    CENTER_ALIGNMENT,
} Text_alignment;

AsyncWebServer server(80);

GxIO_Class io(SPI, ELINK_SS, ELINK_DC, ELINK_RESET);
GxEPD_Class display(io, ELINK_RESET, ELINK_BUSY);

Badge_Info_t info;
static const uint16_t input_buffer_pixels = 20;       // may affect performance
static const uint16_t max_palette_pixels = 256;       // for depth <= 8
uint8_t mono_palette_buffer[max_palette_pixels / 8];  // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint8_t input_buffer[3 * input_buffer_pixels];        // up to depth 24
const char *path[2] = {DEFALUT_AVATAR_BMP, DEFALUT_QR_CODE_BMP};

Button2 *pBtns = nullptr;
uint8_t g_btns[] = BUTTONS_MAP;

File file2;
BookInfo currentBookInfo = BookInfo();

int PAGE_SIZE = 200;

void button_handle(uint8_t gpio)
{
    switch (gpio)
    {
#if BUTTON_3
    case BUTTON_3:
    {
        currentBookInfo.position = currentBookInfo.nextPagePosition;
        renderPage();
    }
    break;
#endif
    default:
        break;
    }
}

void button_callback(Button2 &b)
{
    for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i)
    {
        if (pBtns[i] == b)
        {
            Serial.printf("btn: %u press\n", pBtns[i].getAttachPin());
            button_handle(pBtns[i].getAttachPin());
        }
    }
}

void button_init()
{
    uint8_t args = sizeof(g_btns) / sizeof(g_btns[0]);
    pBtns = new Button2[args];
    for (int i = 0; i < args; ++i)
    {
        pBtns[i] = Button2(g_btns[i]);
        pBtns[i].setPressedHandler(button_callback);
    }
}

void button_loop()
{
    for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i)
    {
        pBtns[i].loop();
    }
}

/**
 * Returns the maximum number of characters which can fit on screen from this string.
 */
int getMaxTextLength(const String &str)
{
    int MAX_HEIGHT = 200;
    String copy = "" + str;
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(copy, 0, 0, &x1, &y1, &w, &h);
    int i = 0;
    while (h > MAX_HEIGHT)
    {
        copy[str.length() - i++] = 0;
        display.getTextBounds(copy, 0, 0, &x1, &y1, &w, &h);
    }
    // Now back up to a word boundry
    while (copy[str.length() - i] != ' ')
        i++;
    return str.length() - i;
}

void displayText(const String &str, int16_t y, uint8_t alignment)
{
    int16_t x = 0;
    int16_t x1, y1;
    uint16_t w, h;
    display.setCursor(x, y);
    display.getTextBounds(str, x, y, &x1, &y1, &w, &h);
    //  Serial.println("text height: " + String(h));
    switch (alignment)
    {
    case RIGHT_ALIGNMENT:
        display.setCursor(display.width() - w - x1, y);
        break;
    case LEFT_ALIGNMENT:
        display.setCursor(0, y);
        break;
    case CENTER_ALIGNMENT:
        display.setCursor(display.width() / 2 - ((w + x1) / 2), y);
        break;
    default:
        break;
    }
    display.println(str);
}

void saveBadgeInfo(Badge_Info_t *info)
{
    // Open file for writing
    File file = FILESYSTEM.open(BADGE_CONFIG_FILE_NAME, FILE_WRITE);
    if (!file)
    {
        Serial.println(F("Failed to create file"));
        return;
    }
#if ARDUINOJSON_VERSION_MAJOR == 5
    StaticJsonBuffer<256> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
#elif ARDUINOJSON_VERSION_MAJOR == 6
    StaticJsonDocument<256> root;
#endif
    // Set the values
    root["company"] = info->company;
    root["name"] = info->name;
    root["address"] = info->address;
    root["email"] = info->email;
    root["link"] = info->link;
    root["tel"] = info->tel;

#if ARDUINOJSON_VERSION_MAJOR == 5
    if (root.printTo(file) == 0)
#elif ARDUINOJSON_VERSION_MAJOR == 6
    if (serializeJson(root, file) == 0)
#endif
    {
        Serial.println(F("Failed to write to file"));
    }
    // Close the file (File's destructor doesn't close the file)
    file.close();
}

bool loadBadgeInfo(Badge_Info_t *info)
{
    if (!FILESYSTEM.exists(BADGE_CONFIG_FILE_NAME))
    {
        Serial.println("load configure fail");
        return false;
    }

    File file = FILESYSTEM.open(BADGE_CONFIG_FILE_NAME);
    if (!file)
    {
        Serial.println("Open Fial -->");
        return false;
    }

#if ARDUINOJSON_VERSION_MAJOR == 5
    StaticJsonBuffer<256> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(file);
    if (!root.success())
    {
        Serial.println(F("Failed to read file, using default configuration"));
        file.close();
        return false;
    }
    root.printTo(Serial);
#elif ARDUINOJSON_VERSION_MAJOR == 6
    StaticJsonDocument<256> root;
    DeserializationError error = deserializeJson(root, file);
    if (error)
    {
        Serial.println(F("Failed to read file, using default configuration"));
        file.close();
        return false;
    }
#endif
    if ((const char *)root["company"] == NULL)
    {
        return false;
    }
    strlcpy(info->company, root["company"], sizeof(info->company));
    strlcpy(info->name, root["name"], sizeof(info->name));
    strlcpy(info->address, root["address"], sizeof(info->address));
    strlcpy(info->email, root["email"], sizeof(info->email));
    strlcpy(info->link, root["link"], sizeof(info->link));
    strlcpy(info->tel, root["tel"], sizeof(info->tel));
    file.close();
    return true;
}

void WebServerStart(void)
{

#ifdef USE_AP_MODE
    uint8_t mac[6];
    char apName[18] = {0};
    IPAddress apIP = IPAddress(192, 168, 1, 1);

    WiFi.mode(WIFI_AP);

    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    esp_wifi_get_mac(WIFI_IF_STA, mac);

    sprintf(apName, "TTGO-Badge-%02X:%02X", mac[4], mac[5]);

    if (!WiFi.softAP(apName))
    {
        Serial.println("AP Config failed.");
        return;
    }
    else
    {
        Serial.println("AP Config Success.");
        Serial.print("AP MAC: ");
        Serial.println(WiFi.softAPmacAddress());
    }
#else
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.print(".");
        esp_restart();
    }
    Serial.println(F("WiFi connected"));
    Serial.println("");
    Serial.println(WiFi.localIP());
#endif

    if (MDNS.begin("ttgo"))
    {
        Serial.println("MDNS responder started");
    }

    server.serveStatic("/", FILESYSTEM, "/").setDefaultFile("index.html");

    server.on("css/main.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(FILESYSTEM, "css/main.css", "text/css");
    });
    server.on("js/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(FILESYSTEM, "js/jquery.min.js", "application/javascript");
    });
    server.on("js/tbdValidate.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(FILESYSTEM, "js/tbdValidate.js", "application/javascript");
    });
    server.on("/data", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "");

        for (int i = 0; i < request->params(); i++)
        {
            String name = request->getParam(i)->name();
            String params = request->getParam(i)->value();
            Serial.println(name + " : " + params);
            if (name == "company")
            {
                strlcpy(info.company, params.c_str(), sizeof(info.company));
            }
            else if (name == "name")
            {
                strlcpy(info.name, params.c_str(), sizeof(info.name));
            }
            else if (name == "address")
            {
                strlcpy(info.address, params.c_str(), sizeof(info.address));
            }
            else if (name == "email")
            {
                strlcpy(info.email, params.c_str(), sizeof(info.email));
            }
            else if (name == "link")
            {
                strlcpy(info.link, params.c_str(), sizeof(info.link));
            }
            else if (name == "tel")
            {
                strlcpy(info.tel, params.c_str(), sizeof(info.tel));
            }
        }
        saveBadgeInfo(&info);
    });

    server.onFileUpload([](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        static File file;
        static int pathIndex = 0;
        if (!index)
        {
            Serial.printf("UploadStart: %s\n", filename.c_str());
            file = FILESYSTEM.open(path[pathIndex], FILE_WRITE);
            if (!file)
            {
                Serial.println("Open FAIL");
                request->send(500, "text/plain", "hander error");
                return;
            }
        }
        if (file.write(data, len) != len)
        {
            Serial.println("Write fail");
            request->send(500, "text/plain", "hander error");
            file.close();
            return;
        }

        if (final)
        {
            Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index + len);
            file.close();
            request->send(200, "text/plain", "");
            if (++pathIndex >= 2)
            {
                pathIndex = 0;
                showMianPage();
            }
        }
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });

    MDNS.addService("http", "tcp", 80);

    server.begin();
}

void displayInit(void)
{
    static bool isInit = false;
    if (isInit)
    {
        return;
    }
    isInit = true;
    display.init();
    display.setRotation(0);
    display.eraseDisplay();
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&DEFALUT_FONT);
    display.setTextSize(0);
}

String BOOK_FILE = "/test.txt";

void setup()
{
    Serial.begin(115200);
    delay(500);

    SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, -1);

    button_init();

    SPIClass sdSPI(VSPI);
    sdSPI.begin(SDCARD_CLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_SS);
    SPIFFS.begin(true);
    displayInit();
    if (!SD.begin(SDCARD_SS, sdSPI))
    {
        Serial.println("No SD Card");
    }
    else
    {
        displayText("Copying Books", 100, CENTER_ALIGNMENT);
        display.updateWindow(0, 0, 128, 250);
        copyFile(BOOK_FILE);
    }
    file2 = SPIFFS.open(BOOK_FILE);

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        currentBookInfo = retriveBookInformation();
        renderPage();
    }

    //  WebServerStart();
}

void copyFile(String filepath)
{

    File sourceFile = SD.open(filepath);
    File destFile = SPIFFS.open(filepath, FILE_WRITE);
    static uint8_t buf[512];
    while (sourceFile.read(buf, 512))
    {
        destFile.write(buf, 512);
    }
    destFile.close();
    sourceFile.close();
}

int getLineHeight(const GFXfont *font = NULL)
{
    int height;
    if (font == NULL)
    {
        height = 12;
    }
    else
    {
        height = (uint8_t)pgm_read_byte(&font->yAdvance);
    }
    return height;
}

const boolean displayPageNumber = false;
void renderStatus()
{
    display.setFont(&FreeSansBold9pt7b);
    display.drawFastHLine(0, 248 - 13, SCREEN_WIDTH, GxEPD_BLACK);
    float battery_voltage = 6.6f * analogRead(BATTERY_ADC) / 4095; // Voltage divider halfs the voltage so saturation is 6.6
    displayText(String(battery_voltage) + "v", 248, LEFT_ALIGNMENT);

    String progress = "";
    if (displayPageNumber)
    {
        progress = String(1 + (currentBookInfo.position / PAGE_SIZE)) + ":" + String(1 + (file2.size() / PAGE_SIZE));
    }
    else
    {
        progress = String(int(round(100.0 / file2.size() * currentBookInfo.position))) + "%";
    }
    displayText(progress, 248, RIGHT_ALIGNMENT);
}

void persistBookInformation(BookInfo &info)
{
    File bookDataFile = SPIFFS.open(BOOK_FILE + ".data", FILE_APPEND);
    int currentPage = bookDataFile.size();
    int n = info.position;
    unsigned char bytes[4];
    bytes[0] = (n >> 24) & 0xFF;
    bytes[1] = (n >> 16) & 0xFF;
    bytes[2] = (n >> 8) & 0xFF;
    bytes[3] = n & 0xFF;
    bookDataFile.write(bytes,4);
    bookDataFile.close();

    File page = SPIFFS.open(BOOK_FILE + ".p.data", FILE_WRITE);
    page.print(String(currentPage));
    page.close();
}

void previousPage(){
    File bookDataFile = SPIFFS.open(BOOK_FILE + ".data", FILE_READ);
    int pos = bookDataFile.size()-5;
    bookDataFile.close();

    File page = SPIFFS.open(BOOK_FILE + ".p.data", FILE_WRITE);
    page.print(String(pos));
    page.close();

}

BookInfo retriveBookInformation()
{
    File bookDataFile = SPIFFS.open(BOOK_FILE + ".data", FILE_READ);
    BookInfo ret = BookInfo();
    if (!bookDataFile)
        return ret;

    File page = SPIFFS.open(BOOK_FILE + ".p.data", FILE_READ);
    bookDataFile.seek(page);
    page.close();

    char bytes[4];
    bookDataFile.readBytes(bytes, 4);
    int n = (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3];
 
    if(n> file2.size()-1 || n <0){
        n=0;
        }
    ret.position = n;
    bookDataFile.close();
    return {ret};
}

// get string length in pixels
// set text font prior to calling this
int getStringLength(String str, int strlength = 0)
{
    int16_t x, y;
    uint16_t w, h;
    display.setTextWrap(false);
    display.getTextBounds(str, 0, 0, &x, &y, &w, &h);
    return (w);
}

// word wrap routine
// first time send string to wrap
// 2nd and additional times: use empty string
// returns substring of wrapped text.
int lineend = 0;
String wrapWord(const char *str, int linesize, bool allowBreak = true)
{
    display.setFont(&DEFALUT_FONT);
    static char buff[1024];
    int linestart = 0;
    // static int lineend = 0;
    static int bufflen = 0;
    if (strlen(str) == 0)
    {
        if (lineend == bufflen)
            return "";
        // additional line from original string
        linestart = lineend + 1;
        lineend = bufflen;
        // Serial.println("existing string to wrap, starting at position " + String(linestart) + ": " + String(&buff[linestart]));
    }
    else
    {
        // Serial.println("new string to wrap: " + String(str));
        memset(buff, 0, sizeof(buff));
        // new string to wrap
        linestart = 0;
        strcpy(buff, str);
        bufflen = strlen(buff);
        lineend=-1;
        return "";
    }

    uint16_t w = 0;
    int lastwordpos = linestart;
    int wordpos = linestart + 1;
    while (true)
    {
        while (buff[wordpos] == ' ' && wordpos < bufflen)
            wordpos++;
        while (buff[wordpos] != ' ' && buff[wordpos] != '\n' && wordpos < bufflen)
            wordpos++;
        char temp = buff[wordpos];
        if (wordpos < bufflen)
        {
            buff[wordpos] = '\0'; // Insert a null for measuring step
        }
        uint16_t lastw = w;
        w = getStringLength(&buff[linestart]);
        // Serial.println(w);
        if (wordpos < bufflen)
        {
            buff[wordpos] = temp; // repair the cut
        }
        if (allowBreak && w > linesize * 1.01 && lastw < linesize * 0.85)
        {
            temp = buff[wordpos];
            buff[wordpos] = 0;
            lineend = wordpos;
            String copy = String(&buff[linestart]);
            int copyLength = copy.length();
            buff[wordpos] = temp;
            int i = 0;
            while (w > linesize)
            {
                copy[copyLength - i++] = 0;
                //Serial.println(copy);
                w = getStringLength(copy);
            }
            copy[copyLength - i] = 0; // One more to make room for the -
            lineend = (wordpos - i - 1);
            int endChar = copy[strlen(&copy[0]) - 1];
            // Serial.println(endChar);
            if (endChar != ' ')
            {
                return String(&copy[0]) + '-';
            }
            w = getStringLength(&buff[linestart]);
        }
        if (w > linesize)
        {
            buff[lastwordpos] = '\0';
            lineend = lastwordpos;
            return &buff[linestart];
        }
        else if (wordpos >= bufflen)
        {
            // first word too long or end of string, send it anyway
            buff[wordpos] = '\0';
            lineend = wordpos;
            return &buff[linestart];
        }
        lastwordpos = wordpos;
        wordpos++;
    }
}

void renderPage()
{
    file2.seek(currentBookInfo.position);
    char text[PAGE_SIZE + 1] = {0};
    for (int i = 0; i < PAGE_SIZE; i++)
    {
        text[i] = file2.read();
    }
    Serial.println(text);
    // int length = getMaxTextLength(text);
    // text[length] = 0;




    display.eraseDisplay(true);
    display.fillScreen(GxEPD_WHITE);


    int y = 15;
    Serial.println("WRAPPING");
    wrapWord(text, 122);
    String line = "-";
    while ((line.length() > 0) && (y == 15 || (display.getCursorY() < 220)))
    {
        int lastY = y;
        line = wrapWord("", 122);
        displayText(line, y, LEFT_ALIGNMENT);
        Serial.println(line);
        if (display.getCursorY() != lastY)
        {
            y = display.getCursorY();
        }
        else
        {
            y += getLineHeight(&DEFALUT_FONT);
        }
    }
    renderStatus();

    display.updateWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    currentBookInfo.nextPagePosition = currentBookInfo.position + lineend + 1;
    persistBookInformation(currentBookInfo);
}

void loop()
{
    yield();
    button_loop();
}