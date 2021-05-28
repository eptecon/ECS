#ifndef THING_SHADOW_ROUTINES
#define THING_SHADOW_ROUTINES

void subscribe_handler(AWS_IoT_Client *pClient, char *pTopicName, uint16_t topicNameLen,
			IoT_Publish_Message_Params *pParams, void *pClientData);

void shadow_get_cb(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
			const char *pReceivedJsonDocument, void *pContextData);

void shadow_init(void);

bool shadow_is_busy(void);

IoT_Error_t update_reported(uint8_t timeout_sec);


#endif /* THING_SHADOW_ROUTINES */
