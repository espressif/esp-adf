#!/usr/bin/python3
# audio_processor.py
import argparse
from audio_test_package.audio_recorder.recorder_stream import Recorder

def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest = "command")

    subparsers.add_parser("rec", help="record sounds")

    parser.add_argument("-i ", "--source-file", help="source file")
    parser.add_argument("-o ", "--target-file", help="target file")
    args = parser.parse_args()

    if args.command:
        recorder = Recorder(filename = 'record.wav')
        recorder.start()
        print("Recording started. Press 'q' to stop recording.")
        while True:
            if input().lower() == 'q':
                break
        recorder.stop()
        recorder.join()
        print("Recording saved as 'record.wav'.")
    else:
        SOURCE_FILE = args.source_file if args.source_file else None
        TARGET_FILE = args.target_file if args.target_file else None
        if not SOURCE_FILE or not TARGET_FILE:
            raise Exception("Source or Target files not specified.")
        audio_similarity_compare(SOURCE_FILE, TARGET_FILE)
        return SOURCE_FILE, TARGET_FILE

def audio_similarity_compare(source_file, target_file):
    correlate(source_file, target_file)

if __name__ == "__main__":
    main()
