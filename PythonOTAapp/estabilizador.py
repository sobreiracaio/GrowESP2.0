from vidstab import VidStab

stabilizer = VidStab()
stabilizer.stabilize(input_path='Video_Casa_REMOVER AUDIO.mp4',
                     output_path='video_estavel.mp4',
                     smoothing_window=30)