# ha-stiebel-control

ha-stiebel-control is a ESPhome / Home Assistant configuration to monitor & configure Stiebel Eltron Heating Pumps via a CAN Interface.
It requires setting up an ESP32 Microcontroller with a MCP2515 CAN-Tranceiver and some configuring in Home Assistant.
It is based on the great work of the Home Assistant community, especially the work of [roberreiters](https://community.home-assistant.io/t/configured-my-esphome-with-mcp2515-can-bus-for-stiebel-eltron-heating-pump/366053) and [Jürg Müller](http://juerg5524.ch/list_data.php).

## Installation

### ESPHome
* Set up a new ESPHome Project "heatingpump"
* Copy the Content of `heatingpump.yaml` to the new project
* copy the folder `stiebeltools`to your `/config/esphome` folder (full path should be `/config/esphome/stiebeltools`)
* Change the WiFi Credentials to yours
* Change the GPIO Pins under `spi` and `can` to your HW configuration
* You may want to check/change the CAN IDs of the Manager, Kessel, etc. In order to do so, you have to change them in two places:
  * `stiebeltools\heatingpump.h`:
    ```c
    static const CanMember CanMembers[] =
    {
    //  Name              CanId     ReadId          WriteId         ConfirmationID
      { "ESPCLIENT"     , 0x700,    {0x00, 0x00},   {0x00, 0x00},   {0xE2, 0x00}}, //The ESP Home Client, thus no valid read/write IDs
      { "KESSEL"        , 0x180,    {0x31, 0x00},   {0x30, 0x00},   {0x00, 0x00}},
      { "MANAGER"       , 0x480,    {0x91, 0x00},   {0x90, 0x00},   {0x00, 0x00}},
      { "HEIZMODUL"     , 0x500,    {0xA1, 0x00},   {0xA0, 0x00},   {0x00, 0x00}}
    };
    ```
  * `heatingpump.yaml`: Look for these blocks in the lower part of the file
    ```yaml
    #########################################
    #                                       #
    #   HEIZMODUL Nachrichten               #
    #                                       #
    #########################################
        - can_id: 0x500
          then:
            - lambda: |-
                unsigned short canId = 500;
    ```

### Home Assistant
#### Entities and Helpers
* place the file `packages/ha_stiebel_control.yaml` in your `/config/packages/` folder in Home Assistant.
* add the package folder to your `configuration.yaml` under `homeassistant` (if not already set up)
```yaml
homeassistant:
  packages: !include_dir_named packages
```
#### Dashboard
* Install the lovelace card [apexcharts-card](https://github.com/RomRider/apexcharts-card)
* Install the lovelace card [lovelace-mushroom](https://github.com/piitaya/lovelace-mushroom)
* Create a new Dashboard, switch to RAW mode and paste the content of `dashboard.yaml`. The result should look similar to this:
![Dashboard Screenshot](assets/img/dashboard.jpg "Dashboard Screenshot")

## Using
* FIXME

## Contributing

Pull requests are welcome. For major changes, please open an issue first
to discuss what you would like to change.

Please make sure to update tests as appropriate.

## License

[GPLv3] (https://www.gnu.org/licenses/gpl-3.0.en.html)