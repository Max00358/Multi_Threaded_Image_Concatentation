# Multi-threaded_Image_Concatentation
## Objective
The objective is to request all horizontal strips of a picture from the server and concatenate them to reconstruct the original image. Given the randomness in strip delivery, utilizing a loop to repeatedly request random strips may result in receiving the same strip multiple times before obtaining all fifty distinct strips. As a consequence of this randomness, the time required to gather all necessary strips for image restoration will vary. 

When accessing the server, the server will randomly display 1 out of 50 images shown as such: \
![image](https://github.com/Max00358/PNG_Finding_and_Concatentation/assets/125518862/a73de071-2c4e-43c6-a5fa-6715001fb8db)

The code will take in the image in a loop and only end when all the images are received from the server and concatenated together into a full, recognizable image like such:

![all](https://github.com/Max00358/PNG_Finding_and_Concatentation/assets/125518862/5939b620-aaf6-48dd-be9d-fbbab4c17f0e)

## Context & Goal
A collection of horizontal strips forming a complete PNG image file was stored on a disk, and the task was to reconstruct the entire image from these strips.

In this lab, the horizontal segments of the image are hosted on the web. Three 400 × 300 pictures (whole images) are distributed across three web servers. When requesting a picture from a web server, the server divides the image into fifty equally sized horizontal strips, each measuring 400 × 6 pixels. The strips are assigned sequence numbers from 0 to 49, indicating their vertical order.

The web server then pauses for a random duration before sending out a random strip in a simplified PNG format, consisting of one IHDR chunk, one IDAT chunk, and one IEND chunk (refer to Figure 1.2(a)). The PNG segment is an 8-bit RGBA/color image (see Table 1.1 for details).

The web server employs an HTTP response header that includes the sequence number to inform the recipient about the sent strip. The HTTP response header follows the format "X-Ece252-Fragment: M," where M ∈ [0, 49]. To request a random horizontal strip of picture N, where N ∈ [1, 3], use the following URL: http://machine:2520/image?img=N, where the machine can be one of the following: ece252-1.uwaterloo.ca, ece252-2.uwaterloo.ca, and ece252-3.uwaterloo.ca. 

For example, when requesting data from the URL http://ece252-1.uwaterloo.ca:2520/image?img=1, you may receive a random horizontal strip of picture 1. Assuming this received strip is the third horizontal strip (from top to bottom of the original picture), the HTTP header will contain "X-Ece252-Fragment: 2". The received data will be in PNG format, and you can use a browser to view the random horizontal strip of the PNG image sent by the server. Notably, hitting enter to refresh the page will display a different image strip each time due to the random nature. Each strip maintains the dimensions of 400 × 6 pixels and is in PNG format. 
