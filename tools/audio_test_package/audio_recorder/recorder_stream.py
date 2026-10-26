# usage: python recorder_stream.py
import os
import signal
import subprocess
import sys
import threading
import requests


class Recorder(threading.Thread):
    '''
    Use the arecord command to record the audio input from the USB sound card and save it as a WAV file.
    '''
    def __init__(self, filename, device='plughw:G3,0', channels=1, sample_rate=44100):
        super().__init__()
        self.filename = filename
        self.device = device
        self.channels = channels
        self.sample_rate = sample_rate
        self.stop_flag = threading.Event()  # Used to control whether recording is stopped
        self.process = None

    def run(self):
        '''
        To start recording, call the arecord command.
        '''
        self.record_audio()

    def stop(self):
        '''
        Stop recording.
        '''
        self.stop_flag.set()  # Set the flag to true to stop recording

        if self.process and self.process.poll() is None:
            try:
                self.process.send_signal(signal.SIGINT)
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
            except Exception:
                pass
        if self.is_alive() and threading.current_thread() is not self:
            self.join(timeout=5)
        print('Recording stopped.', flush=True)

    def record_audio(self):
        '''
        Use the arecord command to record audio and save it as a WAV file.
        '''
        command = [
            'arecord',
            '-D', self.device,      # USB sound card device (assuming it is plusehw:0,0)
            '-f', 'cd',             # CD quality (16-bit, 44100 Hz, mono or stereo)
            '-t', 'wav',            # Output format is WAV
            self.filename           # Output file name
        ]

        try:
            self.process = subprocess.Popen(
                command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            print(f'Recording started and saved to {self.filename}', flush=True)
            self.process.wait()

        except Exception as e:
            print(f'Error while executing arecord: {e}', flush=True)

    def request_audio_similarity(self, url):
        upload_to_similarity_server(self.filename, url)


def upload_to_similarity_server(filename, url):
    file_path = os.path.abspath(filename)
    if not os.path.exists(file_path):
        raise FileNotFoundError(f'Recorded file not found: {file_path}')

    with open(file_path, 'rb') as file:
        files = {'file': (os.path.basename(file.name), file)}
        headers = {
            'accept': 'application/json'
        }

        response = requests.post(url, headers=headers, files=files, timeout=30)

    print(response.text, flush=True)
    if response.status_code != 200:
        raise RuntimeError(f'Request failed, status code:{response.status_code}, content:{response.text}')

    response_data = response.json()
    message = response_data.get('message', '')

    if 'audio_compare_result' in message:
        result = message.split(';')[0].split(':')[1].strip()
        if result == 'fail':
            raise AssertionError(f'audio_compare_result: {result} ({message})')
    else:
        raise RuntimeError(f'Unexpected response format: {response.text}')


def start_recording(filename='output.wav'):
    '''
    Starts recording and allows stopping recording by typing 'q' on the keyboard.
    '''
    recorder = Recorder(filename=filename)
    recorder.start()

    print('Recording started. Press \'q\' to stop recording.')

    # Stop recording by typing 'q'
    while True:
        user_input = input()
        if user_input.lower() == 'q':
            recorder.stop()
            recorder.join()
            print(f'Recording saved as \'{filename}\'.')
            break

if __name__ == '__main__':
    # Start recording, save as 'output.wav' file by default
    start_recording(filename='output.wav')
