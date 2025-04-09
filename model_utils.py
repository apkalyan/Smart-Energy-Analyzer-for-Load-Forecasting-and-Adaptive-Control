from PyQt5.QtCore import QThread, pyqtSignal
from tensorflow.keras.models import load_model
import joblib
import numpy as np
import time

class LSTMThread(QThread):
    forecast_ready = pyqtSignal(float, str)

    def __init__(self, power_data_ref):
        super().__init__()
        self.power_data_ref = power_data_ref
        self.model = load_model("lstm_model.keras")
        self.scaler = joblib.load("scaler.save")
        self.running = True

    def run(self):
        while self.running:
            if len(self.power_data_ref) >= 5:
                try:
                    # Get last 10 values
                    values = [v for (_, v) in self.power_data_ref[-10:]]
                    scaled = self.scaler.transform(np.array(values).reshape(-1, 1))
                    X_input = scaled.reshape(1, 10, 1)
                    prediction = self.model.predict(X_input)
                    forecast = self.scaler.inverse_transform(prediction)[0][0]
                    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
                    self.forecast_ready.emit(forecast, timestamp)
                except Exception as e:
                    print(f"❌ LSTM Forecasting Error: {e}")
            time.sleep(60)  # Wait for 1 minute

    def stop(self):
        self.running = False
        self.wait()
