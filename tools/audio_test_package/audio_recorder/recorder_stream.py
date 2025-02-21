# usage: python recorder_stream.py
import subprocess
import threading
import requests
import sys
import os

from contextlib import redirect_stderr

class Recorder(threading.Thread):
    """
    Use the arecord command to record the audio input from the USB sound card and save it as a WAV file.
    """
    def __init__(self, filename, device='plughw:G3,0', channels=1, sample_rate=44100):
        super().__init__()
        self.filename = filename
        self.device = device
        self.channels = channels
        self.sample_rate = sample_rate
        self.stop_flag = threading.Event()  # Used to control whether recording is stopped
        self.process = None

    def run(self):
        """
        To start recording, call the arecord command.
        """
        self.record_audio()

    def stop(self):
        """
        Stop recording.
        """
        self.stop_flag.set()  # Set the flag to true to stop recording

        # Temporarily redirect error output
        if self.process:
            with open(os.devnull, 'w') as devnull:  # Use 'os.devnull' to discard error output
                with redirect_stderr(devnull):
                    try:
                        self.process.terminate()  # Forced Termination
                        self.process.wait()       # Wait for the process to complete
                    except Exception as e:
                        # Catch exceptions but don't show error output
                        pass
        print("Recording stopped.")

    def record_audio(self):
        """
        Use the arecord command to record audio and save it as a WAV file.
        """
        command = [
            'arecord',
            '-D', self.device,      # USB sound card device (assuming it is plusehw:0,0)
            '-f', 'cd',             # CD quality (16-bit, 44100 Hz, mono or stereo)
            '-t', 'wav',            # Output format is WAV
            self.filename           # Output file name
        ]

        try:
            # Start the arecord command as a subprocess
            self.process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            print(f"Recording started and saved to {self.filename}")

            # Read stdout and stderr streams in real time
            for stdout_line in iter(self.process.stdout.readline, b''):
                sys.stdout.write(stdout_line.decode())

            for stderr_line in iter(self.process.stderr.readline, b''):
                sys.stderr.write(stderr_line.decode())

            self.process.stdout.close()
            self.process.stderr.close()
            self.process.wait()  # Wait for the child process to end

        except Exception as e:
            print(f"Error while executing arecord: {e}")

    def request_audio_similarity(self, url):
        upload_to_similarity_server(self.filename, url)


def upload_to_similarity_server(filename, url):
    file_path = os.path.abspath(filename)
    with open(file_path, 'rb') as file:
        files = {'file': (os.path.basename(file.name), file)}
        headers = {
            'accept': 'application/json'
        }

        response = requests.post(url, headers=headers, files=files)

        # Checking the Status Code
        if response.status_code == 200:
            # Parse the response
            try:
                # Assuming the response is a JSON string like: {"message":"audio_compare_result: success; Similarity: 100.0%"}
                response_data = response.json()
                message = response_data.get('message', '')

                # Check if the message contains "audio_compare_result"
                if 'audio_compare_result' in message:
                    result = message.split(';')[0].split(':')[1].strip()  # Extract the result (success/failed)

                    if result == 'fail':
                        raise Exception(f"audio_compare_result: {result}")
                else:
                    raise Exception("Unexpected response format")

            except Exception as e:
                print(f"Error occurred: {e}")
                os._exit(-1)
            finally:
                print(response.text)
        else:
            print(f"Request failed, status code:{response.status_code}")
            print("Response content:", response.text)
            os._exit(-1)


def start_recording(filename='output.wav'):
    """
    Starts recording and allows stopping recording by typing 'q' on the keyboard.
    """
    recorder = Recorder(filename=filename)
    recorder.start()

    print("Recording started. Press 'q' to stop recording.")

    # Stop recording by typing 'q'
    while True:
        user_input = input()
        if user_input.lower() == 'q':
            recorder.stop()
            recorder.join()
            print(f"Recording saved as '{filename}'.")
            break

if __name__ == '__main__':
    # Start recording, save as 'output.wav' file by default
    start_recording(filename='output.wav')
