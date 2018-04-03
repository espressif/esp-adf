# Play MP3 stream from AWS Polly

The goal of this example is to show how to use Audio Pipeline to play audio generated from text by [Amazon Polly](https://aws.amazon.com/polly/) service. 

To run this example you need ESP32 LyraT or compatible board:
- Connect speakers or headphones to the board. 
- Setup the Wi-Fi connection by `make menuconfig` -> `Example configuration` -> Fill Wi-Fi SSID and Password
- Provide `AWS_ACCESS_KEY` and `AWS_SECRET_KEY`  by `make menuconfig` -> `Example configuration` -> Fill AWS_ACCESS_KEY and AWS_SECRET_KEY value 


For more details on how to use the Amazon Polly service you can refer to [Getting Started with Amazon Polly](https://docs.aws.amazon.com/polly/latest/dg/getting-started.html)
