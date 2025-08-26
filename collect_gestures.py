import cv2
import os
import time

def collect_gesture_images(label, output_dir='dataset', num_images=100, delay=2):
    save_path = os.path.join(output_dir, label)
    os.makedirs(save_path, exist_ok=True)

    cap = cv2.VideoCapture(0)
    print(f"Collecting {num_images} images for gesture '{label}' in {delay} seconds...")

    time.sleep(delay)  # Let user get ready

    count = 0
    while count < num_images:
        ret, frame = cap.read()
        if not ret:
            continue

        # Flip for mirror effect
        frame = cv2.flip(frame, 1)
        cv2.putText(frame, f"Gesture: {label} | Image {count+1}/{num_images}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

        cv2.imshow("Gesture Collector", frame)

        # Save frame
        filename = os.path.join(save_path, f"{label}_{count:04d}.jpg")
        cv2.imwrite(filename, frame)
        count += 1

        # Short pause between captures
        cv2.waitKey(500)  # ~2 fps capture

        if cv2.waitKey(1) & 0xFF == 27:  # ESC to exit early
            break

    print(f"Saved {count} images for '{label}' in '{save_path}'")
    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    gesture_label = input("Enter gesture label (e.g., THUMBS_UP): ").strip().upper()
    collect_gesture_images(gesture_label)
