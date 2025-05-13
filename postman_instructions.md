# ECG Firebase Postman Test Instructions

This guide will help you test your ESP32 ECG device's Firebase connection using Postman.

## Setup

1. **Install Postman**: If you haven't already, download and install Postman from [postman.com](https://www.postman.com/downloads/).

2. **Import the Collection**:
   - Open Postman
   - Click on "Import" button in the upper left
   - Select the `postman_test.json` file
   - Click "Import"

## Available Tests

The collection contains 3 requests:

### 1. Send ECG Data to Firebase
This request simulates what your ESP32 device sends to Firebase:
- Uses the PUT method
- Sends a fixed sample with BPM of 120
- Uses a timestamp generated at the time of request
- Contains rawEcg and smoothedEcg values

### 2. Get ECG Data from Firebase
This request retrieves the current data stored in your Firebase database:
- Uses the GET method
- Returns the most recent data that has been sent to Firebase

### 3. Send Simulated ECG with Random Values
This request simulates the ESP32 sending random ECG data:
- Uses the PUT method
- Generates random values for BPM (100-160)
- Generates random values for rawEcg (1800-2400)
- Generates random values for smoothedEcg (1900-2200)
- Uses a different device ID "postman_test" to distinguish from your ESP32

## Running Tests

1. Select one of the requests in the collection
2. Click the "Send" button
3. View the response in the lower panel
4. The tests will automatically verify that:
   - The status code is 200 (success)
   - The response contains data

## Troubleshooting

If your requests fail, check the following:
- Ensure your Firebase project and Realtime Database are properly set up
- Verify the authentication token is still valid
- Check your internet connection

## Notes

- The Firebase auth token in the collection is the same one used in your ESP32 code
- Each PUT request will overwrite the data at the specified path
- For continuous monitoring, you can run the "Get ECG Data" request repeatedly 