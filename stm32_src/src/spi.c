#include <stdbool.h>

#include "stm32f0xx_spi.h"
#include "spi.h"

#include "tinyprintf.h"
#include "ringbuf.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "main.h"


extern void spi_rx_irq_handler(void);

void spi_init(void) {
	SPI_InitTypeDef spi_slave = {
		.SPI_Direction = SPI_Direction_2Lines_FullDuplex,
		.SPI_Mode = SPI_Mode_Slave,
		.SPI_DataSize = SPI_DataSize_8b,
		.SPI_CPOL = SPI_CPOL_Low,
  		.SPI_CPHA = SPI_CPHA_1Edge,
		.SPI_NSS = SPI_NSS_Hard,
		.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256,
		.SPI_FirstBit = SPI_FirstBit_MSB,
		.SPI_CRCPolynomial = 7,
	};

	GPIO_InitTypeDef gpio_spi2 = {
    		.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15,
    		.GPIO_Mode = GPIO_Mode_AF,
    		.GPIO_OType = GPIO_OType_PP,
    		.GPIO_Speed = GPIO_Speed_50MHz,
		.GPIO_PuPd = GPIO_PuPd_NOPULL,
	};

	GPIO_InitTypeDef gpio_spi2_nss = {
		.GPIO_Pin = GPIO_Pin_12,
		.GPIO_Mode = GPIO_Mode_AF,
		.GPIO_OType = GPIO_OType_PP,
		.GPIO_Speed = GPIO_Speed_50MHz,
		.GPIO_PuPd = GPIO_PuPd_NOPULL,
	};

	/* Configure the SPI interrupt priority */
	NVIC_InitTypeDef spi_nvic = {
		.NVIC_IRQChannel = SPI2_IRQn,
		.NVIC_IRQChannelPriority = 1,
		.NVIC_IRQChannelCmd = ENABLE
	};

	SPI_I2S_DeInit(SPI2);

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

	/* SPI2 */
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF_0);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_0);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_0);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_0);

	GPIO_Init(GPIOB, &gpio_spi2);
	GPIO_Init(GPIOB, &gpio_spi2_nss);

	SPI_Init(SPI2, &spi_slave);

	SPI_Cmd(SPI2, ENABLE);

	NVIC_Init(&spi_nvic);

	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_ERR, ENABLE);


	SPI_RxFIFOThresholdConfig(SPI2, SPI_RxFIFOThreshold_QF);
}

void spi_write_byte(uint8_t data) {
	while ((SPI2->SR & SPI_SR_BSY) == SPI_SR_BSY);
	while ((SPI2->SR & SPI_SR_TXE) != SPI_SR_TXE);
	*((uint8_t *)&SPI2->DR) = data;
}




extern void spi_protocol_slave_do_response(void);

void SPI2_IRQHandler(void) {
	if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_RXNE) == SET) {
		spi_rx_irq_handler();
	}

	if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_TXE) == SET) {
		SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, DISABLE);
	}

	if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_OVR) == SET) {
		SPI_ReceiveData8(SPI2);
		SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_OVR);
	}
}

