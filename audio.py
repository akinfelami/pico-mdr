import wave
import struct
import os

# Define your DAC configuration here (as it will be baked into the data)
# Example for MCP4921, Channel A, 1x Gain, Active
DAC_CONFIG_BITS = 0x3000  # 0b0011000000000000


def wav_to_c_array_preformatted(wav_filepath, output_h_filepath, variable_name="dac_audio_stream"):
    try:
        with wave.open(wav_filepath, 'rb') as wf:
            num_channels = wf.getnchannels()
            sampwidth = wf.getsampwidth()
            framerate = wf.getframerate()
            num_frames = wf.getnframes()
            raw_frame_data = wf.readframes(num_frames)

            print(f"WAV Info:")
            print(f"  Channels: {num_channels}")
            print(f"  Sample Width: {sampwidth*8}-bit")
            print(f"  Frame Rate (Sample Rate): {framerate} Hz")
            print(f"  Number of Frames: {num_frames}")

            if sampwidth != 1 and sampwidth != 2:
                print("ERROR: Only 8-bit or 16-bit WAV files are supported.")
                return

            output_samples_for_dac = []  # This will store the final uint16_t DAC values

            # num_frames is frames, not total samples across channels
            for i in range(num_frames):
                raw_sample_mono = 0
                if sampwidth == 1:  # 8-bit unsigned
                    if num_channels == 1:
                        raw_sample_mono = raw_frame_data[i]
                    else:  # Stereo, average
                        left = raw_frame_data[i * num_channels]
                        right = raw_frame_data[i * num_channels + 1]
                        raw_sample_mono = (int(left) + int(right)) // 2

                    # Scale 8-bit (0-255) to 12-bit (0-4095)
                    scaled_sample_12bit = int(
                        (raw_sample_mono / 255.0) * 4095.0)

                elif sampwidth == 2:  # 16-bit signed
                    if num_channels == 1:
                        frame = raw_frame_data[i*2: i*2+2]
                        raw_sample_mono = struct.unpack('<h', frame)[0]
                    else:  # Stereo, average
                        frame_l = raw_frame_data[i*sampwidth *
                                                 num_channels: i*sampwidth*num_channels+sampwidth]
                        frame_r = raw_frame_data[i*sampwidth*num_channels +
                                                 sampwidth: i*sampwidth*num_channels+(2*sampwidth)]
                        sample_l = struct.unpack('<h', frame_l)[0]
                        sample_r = struct.unpack('<h', frame_r)[0]
                        raw_sample_mono = (sample_l + sample_r) // 2

                    # Scale 16-bit signed (-32768 to 32767) to 12-bit unsigned (0-4095)
                    unsigned_sample_16bit = raw_sample_mono + 32768  # Now 0 to 65535
                    scaled_sample_12bit = int(
                        (unsigned_sample_16bit / 65535.0) * 4095.0)

                # Ensure it's within 0-4095
                scaled_sample_12bit = max(0, min(4095, scaled_sample_12bit))

                # Combine with DAC control bits
                dac_value = DAC_CONFIG_BITS | (scaled_sample_12bit & 0x0FFF)
                output_samples_for_dac.append(dac_value)

            actual_num_samples = len(output_samples_for_dac)

            with open(output_h_filepath, 'w') as hf:
                hf.write(
                    f"// Generated from {os.path.basename(wav_filepath)}\n")
                hf.write(
                    f"// Contains audio data pre-formatted for DAC output (12-bit data + control bits).\n\n")
                hf.write("#ifndef PREFORMATTED_AUDIO_DATA_H\n")
                hf.write("#define PREFORMATTED_AUDIO_DATA_H\n\n")
                hf.write("#include <stdint.h>\n\n")
                hf.write(f"#define AUDIO_SAMPLE_RATE {framerate}\n")
                hf.write(f"#define NUM_AUDIO_SAMPLES {actual_num_samples}\n\n")

                # The array is now const uint16_t, directly usable by DMA from flash
                hf.write(
                    f"const uint16_t {variable_name}[NUM_AUDIO_SAMPLES] = {{\n    ")
                for i, dac_val in enumerate(output_samples_for_dac):
                    hf.write(f"0x{dac_val:04X}")  # Write as hex for clarity
                    if i < actual_num_samples - 1:
                        hf.write(", ")
                    if (i + 1) % 12 == 0 and i < actual_num_samples - 1:  # Newline every 12 samples
                        hf.write("\n    ")
                hf.write("\n};\n\n")
                hf.write("#endif // PREFORMATTED_AUDIO_DATA_H\n")

            print(
                f"\nSuccessfully converted and pre-formatted to {output_h_filepath}")
            print(f"  Output C array type: const uint16_t")
            print(f"  Total Samples in C array: {actual_num_samples}")
            print(f"  Original Sample Rate: {framerate} Hz")

    except FileNotFoundError:
        print(f"Error: WAV file '{wav_filepath}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")


if __name__ == "__main__":
    input_wav = "severance_16bit.wav"  # Your 110510 frame, 48kHz, stereo WAV
    output_h = "audio_data.h"  # New header file name

    if not os.path.exists(input_wav):
        print(f"Input file '{input_wav}' does not exist.")
    else:
        wav_to_c_array_preformatted(input_wav, output_h)
