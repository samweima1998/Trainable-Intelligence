import os
import cv2
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd

# Function to draw lines and collect points
def select_line(event, x, y, flags, param):
    global line_points, drawing, img_copy, img
    if event == cv2.EVENT_LBUTTONDOWN:
        drawing = True
        line_points = [(x, y)]
        img_copy = img.copy()
    elif event == cv2.EVENT_MOUSEMOVE:
        if drawing:
            img_copy = img.copy()
            cv2.line(img_copy, line_points[0], (x, y), (0, 255, 0), 1)
    elif event == cv2.EVENT_LBUTTONUP:
        drawing = False
        line_points.append((x, y))
        cv2.line(img_copy, line_points[0], line_points[1], (0, 255, 0), 1)
        cv2.imshow('Select Line', img_copy)

# Function to prompt the user to redraw lines
def get_user_lines(frame_enhanced):
    global line_points, drawing, img_copy, img
    img = frame_enhanced.copy()
    img_copy = img.copy()
    cv2.namedWindow('Select Line')
    cv2.imshow('Select Line', img)
    cv2.waitKey(1)
    line_points = []
    drawing = False

    # Select the top edge line
    cv2.setMouseCallback('Select Line', select_line)
    print("Draw the line along the top edge of the gap and press 'n' when done.")
    while True:
        cv2.imshow('Select Line', img_copy)
        key = cv2.waitKey(1) & 0xFF
        if key == ord('n') and len(line_points) == 2:
            top_line = line_points.copy()
            break
    img_copy = img.copy()
    line_points = []
    drawing = False

    # Select the bottom edge line
    print("Draw the line along the bottom edge of the gap and press 'n' when done.")
    while True:
        cv2.imshow('Select Line', img_copy)
        key = cv2.waitKey(1) & 0xFF
        if key == ord('n') and len(line_points) == 2:
            bottom_line = line_points.copy()
            break
    cv2.destroyAllWindows()
    return top_line, bottom_line

# Specify the video file path
video_file = 'C:/Users/20195260/OneDrive - TU Eindhoven/Desktop/Inteligent electronics/Video analysis/500mA UV-100mA Blue relax.mp4'

# Create a video capture object
cap = cv2.VideoCapture(video_file)

# Check if the video was opened successfully
if not cap.isOpened():
    print(f"Error opening video file: {video_file}")
    exit()

# Define the frame interval (process every nth frame)
frame_interval = 10

# Read the first frame
ret, frame = cap.read()
if not ret:
    print("Failed to read video.")
    cap.release()
    exit()

# Rotate the frame by 1.5 degrees
rotation_angle =-93  # Positive value rotates counter-clockwise

# Function to rotate image
def rotate_image(image, angle):
    (h, w) = image.shape[:2]
    center = (w // 2, h // 2)
    m = cv2.getRotationMatrix2D(center, angle, 1.0)
    rotated = cv2.warpAffine(image, m, (w, h))
    return rotated

# Function to enhance image by increasing sharpness and contrast
def enhance_image(image):
    lab = cv2.cvtColor(image, cv2.COLOR_BGR2LAB)
    l, a, b = cv2.split(lab)
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    l_clahe = clahe.apply(l)
    lab_clahe = cv2.merge((l_clahe, a, b))
    image_clahe = cv2.cvtColor(lab_clahe, cv2.COLOR_LAB2BGR)
    gaussian = cv2.GaussianBlur(image_clahe, (0, 0), sigmaX=3, sigmaY=3)
    sharpened = cv2.addWeighted(image_clahe, 1.5, gaussian, -0.2, 0)
    return sharpened

# Enhance the first frame
frame_rotated = rotate_image(frame, rotation_angle)
frame_enhanced = enhance_image(frame_rotated)

# Display the enhanced frame for line selection
top_line, bottom_line = get_user_lines(frame_enhanced)

# Prepare for forward processing
cap.set(cv2.CAP_PROP_POS_FRAMES, 0)  # Reset to the beginning
frame_number = 0
times_forward = []
gap_sizes_pixels_forward = []
processed_frame_numbers_forward = []

# Define the scale bar properties
scale_bar_length_um = 100.0  # Length of the scale bar in micrometers
scale_bar_length_pixels = 119  # Length of the scale bar in pixels (measure from the image)

# Calculate the conversion factor from pixels to micrometers
um_per_pixel = scale_bar_length_um / scale_bar_length_pixels

# Initialize tracking state
is_tracking = True

# Function to generate points along a line
def generate_line_points(line, num_points=3000):
    x_coords = np.linspace(line[0][0], line[1][0], num_points)
    y_coords = np.linspace(line[0][1], line[1][1], num_points)
    points = np.vstack((x_coords, y_coords)).T
    return points.astype(np.float32)

# Generate points along the lines
num_points = 3000
search_radius = 2
top_line_points = generate_line_points(top_line, num_points)
bottom_line_points = generate_line_points(bottom_line, num_points)

# Function to detect feature points along the lines
def detect_feature_points(gray_frame, line_points):
    feature_params = dict(maxCorners=1, qualityLevel=0.005, minDistance=1, blockSize=6)
    points = []
    for pt in line_points:
        x, y = int(pt[0]), int(pt[1])
        x_start = max(0, x - search_radius)
        y_start = max(0, y - search_radius)
        x_end = min(gray_frame.shape[1], x + search_radius)
        y_end = min(gray_frame.shape[0], y + search_radius)
        roi = gray_frame[y_start:y_end, x_start:x_end]
        corners = cv2.goodFeaturesToTrack(roi, mask=None, **feature_params)
        if corners is not None:
            corners += np.array([x_start, y_start])
            points.extend(corners.reshape(-1, 2))
    return np.array(points, dtype=np.float32)

# Detect initial feature points on the first frame
gray_initial = cv2.cvtColor(frame_enhanced, cv2.COLOR_BGR2GRAY)
points_top = detect_feature_points(gray_initial, top_line_points)
points_bottom = detect_feature_points(gray_initial, bottom_line_points)

# Initialize the point trackers
lk_params = dict(winSize=(10, 10), maxLevel=2,
                 criteria=(cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 10, 0.03))
prev_gray = gray_initial.copy()
prev_points_top = points_top.reshape(-1, 1, 2)
prev_points_bottom = points_bottom.reshape(-1, 1, 2)

# Variables to handle blue light detection using HSV thresholding
blue_light_on = False  # Current blue light state
blue_light_prev = False  # Previous blue light state

# HSV thresholds for blue light detection
lower_blue = np.array([100, 150, 180])
upper_blue = np.array([120, 255, 255])
blue_area_threshold = 1000  # Adjust based on your video

# Define maximum acceptable displacement (in pixels)
max_displacement = 5  # Adjust this threshold based on your video

# Forward processing loop
while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame_number += 1

    # Process every nth frame
    if (frame_number - 1) % frame_interval != 0:
        continue

    # Rotate and enhance the frame
    rotated_frame = rotate_image(frame, rotation_angle)
    enhanced_frame = enhance_image(rotated_frame)
    frame_gray = cv2.cvtColor(enhanced_frame, cv2.COLOR_BGR2GRAY)

    # Detect blue light using HSV thresholding
    hsv_frame = cv2.cvtColor(rotated_frame, cv2.COLOR_BGR2HSV)
    blue_mask = cv2.inRange(hsv_frame, lower_blue, upper_blue)
    blue_area = cv2.countNonZero(blue_mask)

    # Determine if blue light is currently on
    blue_light_on = blue_area > blue_area_threshold

    # Check for blue light state change
    if blue_light_on and not blue_light_prev:
        # Blue light has just turned on
        print(f"Blue light turned ON at frame {frame_number}")
        # Prompt the user to redraw the lines
        print("Please redraw the top and bottom lines due to blue light.")
        # Allow the user to redraw the lines on the current frame
        top_line, bottom_line = get_user_lines(enhanced_frame)
        # Generate new line points
        top_line_points = generate_line_points(top_line, num_points)
        bottom_line_points = generate_line_points(bottom_line, num_points)
        # Detect feature points along the new lines
        points_top = detect_feature_points(frame_gray, top_line_points)
        points_bottom = detect_feature_points(frame_gray, bottom_line_points)
        # Reinitialize the point trackers
        prev_points_top = points_top.reshape(-1, 1, 2)
        prev_points_bottom = points_bottom.reshape(-1, 1, 2)
        prev_gray = frame_gray.copy()
        is_tracking = True
    elif not blue_light_on and blue_light_prev:
        # Blue light has just turned off
        print(f"Blue light turned OFF at frame {frame_number}")
        # Prompt the user to redraw the lines
        print("Please redraw the top and bottom lines due to blue light turning off.")
        # Allow the user to redraw the lines on the current frame
        top_line, bottom_line = get_user_lines(enhanced_frame)
        # Generate new line points
        top_line_points = generate_line_points(top_line, num_points)
        bottom_line_points = generate_line_points(bottom_line, num_points)
        # Detect feature points along the new lines
        points_top = detect_feature_points(frame_gray, top_line_points)
        points_bottom = detect_feature_points(frame_gray, bottom_line_points)
        # Reinitialize the point trackers
        prev_points_top = points_top.reshape(-1, 1, 2)
        prev_points_bottom = points_bottom.reshape(-1, 1, 2)
        prev_gray = frame_gray.copy()
        is_tracking = True
    else:
        # Continue with the tracking logic
        if is_tracking:
            # Calculate optical flow to track points
            next_points_top, status_top, err_top = cv2.calcOpticalFlowPyrLK(
                prev_gray, frame_gray, prev_points_top, None, **lk_params)
            next_points_bottom, status_bottom, err_bottom = cv2.calcOpticalFlowPyrLK(
                prev_gray, frame_gray, prev_points_bottom, None, **lk_params)

            # Select valid points
            status_top = status_top.reshape(-1)
            status_bottom = status_bottom.reshape(-1)
            prev_points_top_valid = prev_points_top[status_top == 1]
            next_points_top_valid = next_points_top[status_top == 1]
            prev_points_bottom_valid = prev_points_bottom[status_bottom == 1]
            next_points_bottom_valid = next_points_bottom[status_bottom == 1]

            # Check if tracking is lost
            if len(next_points_top_valid) < 2 or len(next_points_bottom_valid) < 2:
                print(f"Tracking lost in frame {frame_number}.")
                is_tracking = False
                gap_size_pixels = 0
                mean_top_y = 0
                mean_bottom_y = 0
                # Store the time and gap size
                time_seconds = frame_number / cap.get(cv2.CAP_PROP_FPS)
                times_forward.append(time_seconds)
                gap_sizes_pixels_forward.append(gap_size_pixels)
                processed_frame_numbers_forward.append(frame_number)
                continue

            # Compute displacement for top points
            displacement_top = np.linalg.norm(next_points_top_valid - prev_points_top_valid, axis=2).reshape(-1)
            # Compute displacement for bottom points
            displacement_bottom = np.linalg.norm(next_points_bottom_valid - prev_points_bottom_valid, axis=2).reshape(-1)

            # Filter out points with abrupt displacement
            displacement_threshold_top = max_displacement
            displacement_threshold_bottom = max_displacement

            good_indices_top = displacement_top <= displacement_threshold_top
            good_indices_bottom = displacement_bottom <= displacement_threshold_bottom

            # Keep only the good points
            prev_points_top_valid = prev_points_top_valid[good_indices_top]
            next_points_top_valid = next_points_top_valid[good_indices_top]
            prev_points_bottom_valid = prev_points_bottom_valid[good_indices_bottom]
            next_points_bottom_valid = next_points_bottom_valid[good_indices_bottom]

            # Update the points for the next iteration
            prev_points_top = next_points_top_valid
            prev_points_bottom = next_points_bottom_valid

            # Ensure there are still enough points after filtering
            if len(next_points_top_valid) < 2 or len(next_points_bottom_valid) < 2:
                print(f"Not enough points after filtering in frame {frame_number}.")
                is_tracking = False
                gap_size_pixels = 0
                mean_top_y = 0
                mean_bottom_y = 0
                # Store the time and gap size
                time_seconds = frame_number / cap.get(cv2.CAP_PROP_FPS)
                times_forward.append(time_seconds)
                gap_sizes_pixels_forward.append(gap_size_pixels)
                processed_frame_numbers_forward.append(frame_number)
                continue

            # Filter out outliers in y-coordinate positions
            # Top edge
            y_top = next_points_top_valid[:, 0, 1]
            y_top_mean = np.mean(y_top)
            y_top_std = np.std(y_top)
            y_top_filtered = y_top[np.abs(y_top - y_top_mean) <= 2 * y_top_std]
            mean_top_y = np.mean(y_top_filtered)

            # Bottom edge
            y_bottom = next_points_bottom_valid[:, 0, 1]
            y_bottom_mean = np.mean(y_bottom)
            y_bottom_std = np.std(y_bottom)
            y_bottom_filtered = y_bottom[np.abs(y_bottom - y_bottom_mean) <= 2 * y_bottom_std]
            mean_bottom_y = np.mean(y_bottom_filtered)

            # Calculate the minimum vertical distance between any pair of points
            y_top = y_top_filtered
            y_bottom = y_bottom_filtered

            # Create all pairs of top and bottom y-coordinates
            y_top_expanded = y_top[:, np.newaxis]
            y_bottom_expanded = y_bottom[np.newaxis, :]

            # Calculate the vertical distances
            vertical_distances = y_bottom_expanded - y_top_expanded

            # Only consider positive distances
            vertical_distances = vertical_distances[vertical_distances >= 0]

            if vertical_distances.size > 0:
                min_distance = np.min(vertical_distances)
            else:
                min_distance = 0

            gap_size_pixels = min_distance

            # Update previous frame and points
            prev_gray = frame_gray.copy()

            # Store the time and gap size
            time_seconds = frame_number / cap.get(cv2.CAP_PROP_FPS)
            times_forward.append(time_seconds)
            gap_sizes_pixels_forward.append(gap_size_pixels)
            processed_frame_numbers_forward.append(frame_number)

        else:
            # Tracking has been lost
            gap_size_pixels = 0
            mean_top_y = 0
            mean_bottom_y = 0
            # Store the time and gap size
            time_seconds = frame_number / cap.get(cv2.CAP_PROP_FPS)
            times_forward.append(time_seconds)
            gap_sizes_pixels_forward.append(gap_size_pixels)
            processed_frame_numbers_forward.append(frame_number)
            continue  # Skip visualization and go to next frame

    # Update the previous blue light state
    blue_light_prev = blue_light_on

    # Visualization of tracked points
    out_frame = rotated_frame.copy()

    # Draw the tracked points (green for top, red for bottom)
    if is_tracking and len(prev_points_top) > 0 and len(prev_points_bottom) > 0:
        for pt in prev_points_top.reshape(-1, 2):
            cv2.circle(out_frame, (int(pt[0]), int(pt[1])), 3, (0, 255, 0), -1)  # Green for top points
        for pt in prev_points_bottom.reshape(-1, 2):
            cv2.circle(out_frame, (int(pt[0]), int(pt[1])), 3, (0, 0, 255), -1)  # Red for bottom points

    # Draw lines across the whole frame representing the mean positions of the edges
    x1 = 0
    x2 = out_frame.shape[1]
    if mean_top_y != 0:
        cv2.line(out_frame, (x1, int(mean_top_y)), (x2, int(mean_top_y)), (0, 255, 255), 2)  # Yellow for top line
    if mean_bottom_y != 0:
        cv2.line(out_frame, (x1, int(mean_bottom_y)), (x2, int(mean_bottom_y)), (255, 255, 0), 2)  # Cyan for bottom line

    # Display the gap size in the top-left corner
    gap_size_um = gap_size_pixels * um_per_pixel
    cv2.putText(out_frame, f'Gap Size: {gap_size_um:.2f} μm', (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2)

    # Show the frame with tracked points, lines, and gap size
    cv2.imshow('Gap Tracking', out_frame)
    key = cv2.waitKey(1) & 0xFF
    if key == 27:  # Press 'Esc' to exit
        break

# Release the video capture object
cap.release()
cv2.destroyAllWindows()

# Convert gap sizes to micrometers
gap_sizes_um = [size * um_per_pixel for size in gap_sizes_pixels_forward]

# Collecting data into a dictionary
data = {
        "Gap Size (μm)": gap_sizes_um,
        "Time (s)": times_forward,
        "Frame Number": processed_frame_numbers_forward
}

# Create a pandas DataFrame from the collected data
df = pd.DataFrame(data)

# Use the video file name to derive the output file path
video_name = os.path.splitext(os.path.basename(video_file))[0]

# Set the output folder as specified
output_folder = 'C:/Users/20195260/OneDrive - TU Eindhoven/Desktop/Inteligent electronics/Video analysis'
output_path = os.path.join(output_folder, f"{video_name}_500mA UV-100mA Blue relax.xlsx")

# Save the data to an Excel file
df.to_excel(output_path, index=False)
print(f"Data exported to {output_path}")

# Plotting
plt.figure()
plt.plot(times_forward, gap_sizes_um, '-o')
plt.xlabel('Time (s)')
plt.ylabel('Gap Size (μm)')
plt.title('Gap Size Over Time')
plt.grid(True)
plt.show()
