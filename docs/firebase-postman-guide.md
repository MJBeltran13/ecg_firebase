# Firebase Integration Testing with Postman

This guide shows how to use Postman to test the Firebase Realtime Database integration for the ECG monitoring system.

## Prerequisites

1. [Postman](https://www.postman.com/downloads/) installed
2. Access to the Firebase Realtime Database at: 
   `https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/`

## Testing Data Submission

### 1. Setting up a PUT Request

Create a new request in Postman with the following configuration:

- **Method**: PUT
- **URL**: `https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json`

Note: The `.json` extension at the end is required for Firebase REST API calls.

### 2. Adding Request Body

Configure the request body:

1. Select the "Body" tab
2. Choose "raw" and select "JSON" from the dropdown
3. Enter the following JSON:

```json
{
  "deviceId": "esp32",
  "bpm": 75,
  "timestamp": "2023-06-01T12:30:45",
  "rawEcg": 512,
  "smoothedEcg": 30
}
```

### 3. Send the Request

Click the "Send" button. If successful, you should receive a response confirming the data was written.

## Simulating the Arduino Device

To simulate data that would be sent from the Arduino ESP32 device:

1. Create a new PUT request to the same URL:
   `https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json`

2. Body:
```json
{
  "deviceId": "esp32",
  "bpm": 78,
  "timestamp": "2023-06-01T12:35:00",
  "rawEcg": 523,
  "smoothedEcg": 35
}
```

Note: With this approach, each new PUT request will replace the existing data in the Firebase database.

## Reading Data

To read the ECG reading:

1. Create a GET request
2. URL: `https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json`
3. Send the request to see the stored reading

## Creating a Postman Collection

For convenience, create a Postman Collection with these requests:

1. "Submit ECG Reading" (PUT)
2. "Read ECG Reading" (GET)

Save the collection for easy access and share with team members.

## Testing Schedule

To verify the integration works properly, perform these tests:

1. Submit test data using Postman
2. Verify the data appears in the app
3. Once Arduino code is deployed, verify real device data appears correctly

## Troubleshooting

If you encounter errors:

- Check that you're including `.json` at the end of Firebase REST API URLs
- Verify the timestamp format is correct (YYYY-MM-DDThh:mm:ss)
- Ensure you have proper read/write permissions for the Firebase database
- Remember that each PUT to this URL will replace the existing data
- For more complex data storage, consider using a POST request to store data at a child node 