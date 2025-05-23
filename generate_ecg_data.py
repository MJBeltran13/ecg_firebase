import requests
import random
from datetime import datetime
import time

# Firebase URL
FIREBASE_URL = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json"
AUTH_TOKEN = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM"

def generate_ecg_data():
    # Generate random BPM between 130-140 as integer
    bpm = random.randint(130, 140)
    
    # Generate simulated ECG values as integers
    raw_ecg = random.randint(90, 100)  # Random raw ECG value
    smoothed_ecg = raw_ecg + random.randint(-5, 5)  # Smoothed version with some variation
    
    # Current timestamp in ISO format
    timestamp = datetime.now().isoformat()
    
    data = {
        "deviceId": "esp32",
        "bpm": bpm,
        "timestamp": timestamp,
        "rawEcg": raw_ecg,
        "smoothedEcg": smoothed_ecg
    }
    
    return data

def send_to_firebase(data):
    url = f"{FIREBASE_URL}?auth={AUTH_TOKEN}"
    response = requests.put(url, json=data)
    
    if response.status_code == 200:
        print(f"Successfully sent data: {data}")
    else:
        print(f"Error sending data: {response.status_code}")
        print(response.text)

def main():
    try:
        while True:
            data = generate_ecg_data()
            send_to_firebase(data)
            time.sleep(2)  # Wait for 2 seconds before sending next data point
    except KeyboardInterrupt:
        print("\nStopping data generation...")

if __name__ == "__main__":
    main() 