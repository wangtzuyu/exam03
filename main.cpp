#include "mbed.h"
// for RPC
#include "mbed_rpc.h"
// for WIFI
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
// for ACC
#include "fsl_port.h"
#include "fsl_gpio.h"
#define UINT14_MAX 16383
#define FXOS8700CQ_SLAVE_ADDR0 (0x1E << 1) // with pins SA0=0, SA1=0
#define FXOS8700CQ_SLAVE_ADDR1 (0x1D << 1) // with pins SA0=1, SA1=0
#define FXOS8700CQ_SLAVE_ADDR2 (0x1C << 1) // with pins SA0=0, SA1=1
#define FXOS8700CQ_SLAVE_ADDR3 (0x1F << 1) // with pins SA0=1, SA1=1
#define FXOS8700Q_STATUS 0x00
#define FXOS8700Q_OUT_X_MSB 0x01
#define FXOS8700Q_OUT_Y_MSB 0x03
#define FXOS8700Q_OUT_Z_MSB 0x05
#define FXOS8700Q_M_OUT_X_MSB 0x33
#define FXOS8700Q_M_OUT_Y_MSB 0x35
#define FXOS8700Q_M_OUT_Z_MSB 0x37
#define FXOS8700Q_WHOAMI 0x0D
#define FXOS8700Q_XYZ_DATA_CFG 0x0E
#define FXOS8700Q_CTRL_REG1 0x2A
#define FXOS8700Q_M_CTRL_REG1 0x5B
#define FXOS8700Q_M_CTRL_REG2 0x5C
#define FXOS8700Q_WHOAMI_VAL 0xC7
I2C i2c(PTD9, PTD8);
int m_addr = FXOS8700CQ_SLAVE_ADDR1;
uint8_t who_am_i, data[2], res[6];
int16_t acc16;
float tacc[3];
float tacc_back[3];
// for xbee & pc connection
RawSerial pc(USBTX, USBRX);
RawSerial xbee(D12, D11);
EventQueue queue(32 * EVENTS_EVENT_SIZE);
Thread t;
// for WIFI setting
WiFiInterface *wifi;
InterruptIn btn2(SW2);
volatile int arrivedcount = 0;
volatile bool closed = false;
const char *topic = "velocity";
// function list
void xbee_rx_interrupt(void);
void xbee_rx(void);
void reply_messange(char *xbee_reply, char *messange);
void check_addr(char *xbee_reply, char *messenger);
void FXOS8700CQ_readRegs(int addr, uint8_t *data, int len);
void FXOS8700CQ_writeRegs(uint8_t *data, int len);
void messageArrived(MQTT::MessageData &md);
void publish_message(MQTT::Client<MQTTNetwork, Countdown> *client);
// for RPC function
void ACC(Arguments *in, Reply *out);
RPCFunction rpcLED(&ACC, "ACC");
int countnum = 0;
int count_array[20];
int j = 0;
int m = 0;
int num_count = 0;
// wifi data count
float sec_pass = 0;
int main()
{
   // initialization setting
   pc.baud(9600);
   xbee.baud(9600);
   // for ACC
   FXOS8700CQ_readRegs(FXOS8700Q_CTRL_REG1, &data[1], 1);
   data[1] |= 0x01;
   data[0] = FXOS8700Q_CTRL_REG1;
   FXOS8700CQ_writeRegs(data, 2);
   FXOS8700CQ_readRegs(FXOS8700Q_WHOAMI, &who_am_i, 1);
   // for Xbee
   char xbee_reply[4];
   xbee.printf("+++");
   xbee_reply[0] = xbee.getc();
   xbee_reply[1] = xbee.getc();
   if (xbee_reply[0] == 'O' && xbee_reply[1] == 'K')
   {
      pc.printf("enter AT mode.\r\n");
      xbee_reply[0] = '\0';
      xbee_reply[1] = '\0';
   }
   xbee.printf("ATMY 0x240\r\n");
   reply_messange(xbee_reply, "setting MY : 0x240");
   xbee.printf("ATDL 0x140\r\n");
   reply_messange(xbee_reply, "setting DL : 0x140");
   xbee.printf("ATID 0x1\r\n");
   reply_messange(xbee_reply, "setting PAN ID : 0x1");
   xbee.printf("ATWR\r\n");
   reply_messange(xbee_reply, "write config");
   xbee.printf("ATMY\r\n");
   check_addr(xbee_reply, "MY");
   xbee.printf("ATDL\r\n");
   check_addr(xbee_reply, "DL");
   xbee.printf("ATCN\r\n");
   reply_messange(xbee_reply, "exit AT mode");
   xbee.getc();
   // wifi initialization
   wifi = WiFiInterface::get_default_instance();
   if (!wifi)
   {
      printf("ERROR: No WiFiInterface found.\r\n");
      return -1;
   }
   printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
   int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
   if (ret != 0)
   {
      printf("\nConnection error: %d\r\n", ret);
      return -1;
   }
   NetworkInterface *net = wifi;
   MQTTNetwork mqttNetwork(net);
   MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);
   const char *host = "192.168.1.117";
   printf("Connecting to TCP network...\r\n");
   int rc = mqttNetwork.connect(host, 1883);
   if (rc != 0)
   {
      printf("Connection error.");
      return -1;
   }
   printf("Successfully connected!\r\n");
   MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
   data.MQTTVersion = 3;
   data.clientID.cstring = "Mbed";
   if ((rc = client.connect(data)) != 0)
   {
      printf("Fail to connect MQTT\r\n");
   }
   if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0)
   {
      printf("Fail to subscribe\r\n");
   }
   // send data through WIFI
   while (sec_pass <= 20)
   {
      if (tacc[2] < 0)
      {
         if (tacc_back[2] > 0)
         {
            if (int(sec_pass * 10.0) % 10 == 0)
               {
                  count_array[j] = countnum;
                  countnum = 0;
                  j++;
               }
            for (int k = 0; k < 10; k++)
            {
               countnum++;
               // reading ACC data
               FXOS8700CQ_readRegs(FXOS8700Q_OUT_X_MSB, res, 6);
               acc16 = (res[0] << 6) | (res[1] >> 2);
               if (acc16 > UINT14_MAX / 2)
                  acc16 -= UINT14_MAX;
               tacc_back[0] = tacc[0];
               tacc[0] = ((float)acc16) / 4096.0f;
               acc16 = (res[2] << 6) | (res[3] >> 2);
               if (acc16 > UINT14_MAX / 2)
                  acc16 -= UINT14_MAX;
               tacc_back[1] = tacc[1];
               tacc[1] = ((float)acc16) / 4096.0f;
               acc16 = (res[4] << 6) | (res[5] >> 2);
               if (acc16 > UINT14_MAX / 2)
                  acc16 -= UINT14_MAX;
               tacc_back[2] = tacc[2];
               tacc[2] = ((float)acc16) / 4096.0f;
               sec_pass = sec_pass + 0.1;
               publish_message(&client);
               wait(0.1);
            }
            if (int(sec_pass * 10.0) % 10 == 0)
               {
                  count_array[j] = countnum;
                  countnum = 0;
                  j++;
               }
         }
         else
         {
            countnum++;
            if (int(sec_pass * 10.0) % 10 == 0)
            {
               count_array[j] = countnum;
               countnum = 0;
               j++;
            }
            // reading ACC data
            FXOS8700CQ_readRegs(FXOS8700Q_OUT_X_MSB, res, 6);
            acc16 = (res[0] << 6) | (res[1] >> 2);
            if (acc16 > UINT14_MAX / 2)
               acc16 -= UINT14_MAX;
            tacc_back[0] = tacc[0];
            tacc[0] = ((float)acc16) / 4096.0f;
            acc16 = (res[2] << 6) | (res[3] >> 2);
            if (acc16 > UINT14_MAX / 2)
               acc16 -= UINT14_MAX;
            tacc_back[1] = tacc[1];
            tacc[1] = ((float)acc16) / 4096.0f;
            acc16 = (res[4] << 6) | (res[5] >> 2);
            if (acc16 > UINT14_MAX / 2)
               acc16 -= UINT14_MAX;
            tacc_back[2] = tacc[2];
            tacc[2] = ((float)acc16) / 4096.0f;
            sec_pass = sec_pass + 0.5;
            publish_message(&client);
            wait(0.5);
         }
      }
      else
      {
         countnum++;
         if (int(sec_pass * 10.0) % 10 == 0)
         {
            count_array[j] = countnum;
            countnum = 0;
            j++;
         }
         // reading ACC data
         FXOS8700CQ_readRegs(FXOS8700Q_OUT_X_MSB, res, 6);
         acc16 = (res[0] << 6) | (res[1] >> 2);
         if (acc16 > UINT14_MAX / 2)
            acc16 -= UINT14_MAX;
         tacc_back[0] = tacc[0];
         tacc[0] = ((float)acc16) / 4096.0f;
         acc16 = (res[2] << 6) | (res[3] >> 2);
         if (acc16 > UINT14_MAX / 2)
            acc16 -= UINT14_MAX;
         tacc_back[1] = tacc[1];
         tacc[1] = ((float)acc16) / 4096.0f;
         acc16 = (res[4] << 6) | (res[5] >> 2);
         if (acc16 > UINT14_MAX / 2)
            acc16 -= UINT14_MAX;
         tacc_back[2] = tacc[2];
         tacc[2] = ((float)acc16) / 4096.0f;
         sec_pass = sec_pass + 0.5;
         publish_message(&client);
         wait(0.5);
      }
   }
   // start xbee communication thread
   pc.printf("start xbee communication\r\n");
   t.start(callback(&queue, &EventQueue::dispatch_forever));
   // Setup a serial interrupt function of receiving data from xbee
   xbee.attach(xbee_rx_interrupt, Serial::RxIrq);
}
// xbee communication setting function
void xbee_rx_interrupt(void)
{
   xbee.attach(NULL, Serial::RxIrq); // detach interrupt
   queue.call(&xbee_rx);
}
// xbee command core
void xbee_rx(void)
{
   char buf[100] = {0};
   char outbuf[100] = {0};
   while (xbee.readable())
   {
      for (int i = 0;; i++)
      {
         char recv = xbee.getc();
         if (recv == '\r')
         {
            break;
         }
         buf[i] = recv;
      }
      pc.printf("%s\r\n", buf);
      RPC::call(buf, outbuf);
      wait(0.001);
   }
   xbee.attach(xbee_rx_interrupt, Serial::RxIrq); // reattach interrupt
}
void reply_messange(char *xbee_reply, char *messange)
{
   xbee_reply[0] = xbee.getc();
   xbee_reply[1] = xbee.getc();
   xbee_reply[2] = xbee.getc();
   if (xbee_reply[1] == 'O' && xbee_reply[2] == 'K')
   {
      pc.printf("%s\r\n", messange);
      xbee_reply[0] = '\0';
      xbee_reply[1] = '\0';
      xbee_reply[2] = '\0';
   }
}
void check_addr(char *xbee_reply, char *messenger)
{
   xbee_reply[0] = xbee.getc();
   xbee_reply[1] = xbee.getc();
   xbee_reply[2] = xbee.getc();
   xbee_reply[3] = xbee.getc();
   pc.printf("%s = %c%c%c\r\n", messenger, xbee_reply[1], xbee_reply[2], xbee_reply[3]);
   xbee_reply[0] = '\0';
   xbee_reply[1] = '\0';
   xbee_reply[2] = '\0';
   xbee_reply[3] = '\0';
}
// RPC function
void ACC(Arguments *in, Reply *out)
{
   if (count_array[m] < 10)
   {
      xbee.printf("0%d", count_array[m]);
   }
   else
   {
      xbee.printf("%d", count_array[m]);
   }
   m++;
}
// ACC setting function
void FXOS8700CQ_readRegs(int addr, uint8_t *data, int len)
{
   char t = addr;/*  */
   i2c.write(m_addr, &t, 1, true);
   i2c.read(m_addr, (char *)data, len);
}
void FXOS8700CQ_writeRegs(uint8_t *data, int len)
{
   i2c.write(m_addr, (char *)data, len);
}
// WIFI function
void messageArrived(MQTT::MessageData &md)
{
   MQTT::Message &message = md.message;
   char msg[300];
   sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);
   printf(msg);
   wait_ms(1000);
   char payload[300];
   sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char *)message.payload);
   printf(payload);
   ++arrivedcount;
}
void publish_message(MQTT::Client<MQTTNetwork, Countdown> *client)
{
   // message sending
   num_count++;
   MQTT::Message message;
   char buff[100];
   sprintf(buff, "%d#%1.2f#%1.2f#%1.2f#%1.2f#", num_count, sec_pass, tacc[0], tacc[1], tacc[2]);
   message.qos = MQTT::QOS0;
   message.retained = false;
   message.dup = false;
   message.payload = (void *)buff;
   message.payloadlen = strlen(buff) + 1;
   int rc = client->publish(topic, message);
   printf("rc= %d Puslish message: %s\r\n", rc, buff);
}
