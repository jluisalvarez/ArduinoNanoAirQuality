#include <ArduinoBLE.h>

#include <MaximWire.h>

#include <mbed.h>
#include <BMP280_SPI.h>

using namespace mbed;

#include <DHT.h>

#include <Adafruit_CCS811.h>
#include <Wire.h>
#include <BH1750.h>

// DS18B20:  if you use only one sensor, you don't need external pull-up resistor. Just connect directly: 3.3v, GND and PIN D2
#define PIN_BUS D2
MaximWire::Bus bus(PIN_BUS);
MaximWire::DS18B20 ds18b20;

// DHT22
#define DHTPIN D4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// BMP280
PinName BMP_CS = P0_27;
PinName BMP_SCK = P0_13;
PinName BMP_MISO = P1_8;
PinName BMP_MOSI = P1_1;

BMP280_SPI bmp(BMP_MOSI, BMP_MISO, BMP_SCK, BMP_CS);

// CCS811
Adafruit_CCS811 ccs;

BH1750 lightMeter;

float temperature = 0;
float humidity = 0;
float pressure = 0;
float co2 = 0;
float tvoc = 0;
float co = 0;
float ambientLight = 0;



// Custom Service Air Quality Sensing 273c51b8-181A-0000-8f98-0c43b9fa3391
BLEService airqualitySensingService("273c51b8-181A-0000-8f98-0c43b9fa3391");

// Characteristic 
BLEFloatCharacteristic tempCharacteristic("273c51b8-181A-0001-8f98-0c43b9fa3391", BLERead | BLENotify);
BLEFloatCharacteristic humidCharacteristic("273c51b8-181A-0002-8f98-0c43b9fa3391", BLERead | BLENotify);
BLEFloatCharacteristic pressCharacteristic("273c51b8-181A-0003-8f98-0c43b9fa3391", BLERead | BLENotify);
BLEFloatCharacteristic co2Characteristic("273c51b8-181A-0004-8f98-0c43b9fa3391", BLERead | BLENotify);
BLEFloatCharacteristic tvocCharacteristic("273c51b8-181A-0005-8f98-0c43b9fa3391", BLERead | BLENotify);
BLEFloatCharacteristic coCharacteristic("273c51b8-181A-0006-8f98-0c43b9fa3391", BLERead | BLENotify);
BLEFloatCharacteristic luxCharacteristic("273c51b8-181A-0007-8f98-0c43b9fa3391", BLERead | BLENotify);




void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(5000);

  Serial.begin(9600);

  Serial.println("Setup sensors");

  Wire.begin();

  lightMeter.begin();

  int i=0;
  while (!ccs.begin() && i<5) {
    Serial.println("Failed to start sensor! Please check your wiring.");
    delay(2000);
    i++;
  }

  // Wait for the sensor to be ready
  while (!ccs.available());


  Serial.println("Initializing Bluetooth communication.");
  if (!BLE.begin()) {
    Serial.println("Failed.");
    //error_pulse();
  }



  // Set up Bluetooth Environmental Sensing service.
  Serial.println("Setting up service with characteristics ....");
  BLE.setLocalName("Nano33BLE");
  BLE.setAdvertisedService(airqualitySensingService);

  // Add characteristics for barometric pressure, temperature, and humidity.
  airqualitySensingService.addCharacteristic(tempCharacteristic);
  airqualitySensingService.addCharacteristic(humidCharacteristic);
  airqualitySensingService.addCharacteristic(pressCharacteristic);
  airqualitySensingService.addCharacteristic(co2Characteristic);
  airqualitySensingService.addCharacteristic(tvocCharacteristic);
  airqualitySensingService.addCharacteristic(coCharacteristic);
  airqualitySensingService.addCharacteristic(luxCharacteristic);

  // Make the service available.
  BLE.addService(airqualitySensingService);
  BLE.setConnectable(true);
  Serial.println("Advertising services.");
  BLE.advertise();

  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {

  // Get readings from sensors 
  readSensors();

  BLEDevice central = BLE.central();


  if (central) {
	// Turn on LED when connection is established.
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.print("Incoming connection from: ");
    Serial.println(central.address());

    while (central.connected()) {
      
      // Update Bluetooth characteristics with new values.
      tempCharacteristic.writeValue(temperature);
      humidCharacteristic.writeValue(humidity);
      pressCharacteristic.writeValue(pressure);
      co2Characteristic.writeValue(co2);
      tvocCharacteristic.writeValue(tvoc);
      coCharacteristic.writeValue(co);
      luxCharacteristic.writeValue(ambientLight);

      // Delay between updates. (Don't make too long of connections start to timeout.)
      delay(1000);
    }
    // Turn off LED when connection is dropped.
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Connection terminated.");
  }
  // Delay between reading sensors 
  delay(2000);
}


void readSensors() {

  // Write values to serial port for debug.

  if (ds18b20.IsValid()) {
    temperature = ds18b20.GetTemperature<float>(bus);
    if (!isnan(temperature)) {
      Serial.print("Temperatura: ");
      Serial.print(temperature);
      Serial.println(" ºC");
      ds18b20.Update(bus);
    } else {
      Serial.print("LOST ");
      Serial.println(ds18b20.ToString());
      ds18b20.Reset();
    }
  } else {
    if (bus.Discover().FindNextDevice(ds18b20) && ds18b20.GetModelCode() == MaximWire::DS18B20::MODEL_CODE) {
      Serial.print("FOUND ");
      Serial.println(ds18b20.ToString());
      ds18b20.Update(bus);
    } else {
      Serial.println("NOTHING FOUND");
      ds18b20.Reset();
    }
  }

  float tDHT22 = dht.readTemperature();
  float hDHT22 = dht.readHumidity();

 
  if ( isnan(tDHT22) || isnan(hDHT22) ) {
    Serial.println(F("Failed to read from DHT sensor!"));

  } else {
    humidity = hDHT22;
  }
    Serial.print(F("Humidity = "));
    Serial.print(humidity);
    Serial.println(" %");
  

  pressure = bmp.getPressure();
  Serial.print(F("Pressure = "));
  Serial.print(pressure);
  Serial.println(" hPa");


  if (ccs.available()) {
    if (!ccs.readData()) {
      co2 = ccs.geteCO2();
      tvoc = ccs.getTVOC();
      Serial.print(" CO2: ");
      Serial.print(co2);
      Serial.print(" ppm, TVOC: ");
      Serial.print(tvoc);
      Serial.println(" ppm");
    } else {
      Serial.println("ERROR");
    }
  }

  int adc_MQ = analogRead(A0); 
  float voltaje = adc_MQ * (3.3 / 4096.0); 
  float Rs=1000*((3.3-voltaje)/voltaje);  

  co = 22.94*pow(Rs/5463, -1.497);
  
  Serial.print("  CO:");
  Serial.print(co);
  Serial.println(" ppm");

  ambientLight = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(ambientLight);
  Serial.println(" lx");

  Serial.println("");

}
