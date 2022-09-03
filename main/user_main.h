typedef struct
{
    uint8_t timestamp;
    uint8_t temperature;
    uint8_t timezone;
    uint8_t content[200];
    uint8_t network_ConnetFlag;
} variable, *var;