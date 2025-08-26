import os
import cv2
import pandas as pd
import mediapipe as mp

# Setup MediaPipe Hands
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(static_image_mode=True, max_num_hands=1)
    
def extract_landmarks_from_image(image_path):
    image = cv2.imread(image_path)
    image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)

    results = hands.process(image_rgb)
    if not results.multi_hand_landmarks:
        return None  # No hand detected

    # Extract landmark coordinates (x, y, z for each of 21 points)
    landmarks = results.multi_hand_landmarks[0].landmark
    return [coord for lm in landmarks for coord in (lm.x, lm.y, lm.z)]

def process_dataset(dataset_path='dataset'):
    data = []
    for gesture_label in os.listdir(dataset_path):
        gesture_dir = os.path.join(dataset_path, gesture_label)
        if not os.path.isdir(gesture_dir):
            continue
        for filename in os.listdir(gesture_dir):
            if not filename.lower().endswith(('.png', '.jpg', '.jpeg')):
                continue
            image_path = os.path.join(gesture_dir, filename)
            landmarks = extract_landmarks_from_image(image_path)
            if landmarks:
                data.append([gesture_label] + landmarks)
            else:
                print(f"⚠️ No hand detected in: {image_path}")

    # Create dataframe and save to CSV
    landmark_names = [f'{axis}{i}' for i in range(21) for axis in ['x','y','z']]
    df = pd.DataFrame(data, columns=['gesture'] + landmark_names)
    df.to_csv('gesture_landmarks.csv', index=False)
    print(f"✅ Landmarks saved to gesture_landmarks.csv with {len(df)} valid samples.")

if __name__ == "__main__":
    process_dataset()
