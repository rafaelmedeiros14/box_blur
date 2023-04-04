#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

using namespace std;

// CONSTANTS
static const string INPUT_DIRECTORY = "../input";
static const string OUTPUT_DIRECTORY = "../output";
static const int FILTER_SIZE = 5;
static const int NUM_CHANNELS = 3;

// Image type definition
typedef vector<vector<uint8_t>> single_channel_image_t;
typedef array<single_channel_image_t, NUM_CHANNELS> image_t;

image_t load_image(const string &filename)
{
    int width, height, channels;

    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        throw runtime_error("Failed to load image " + filename);
    }

    image_t result;
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        result[i] = single_channel_image_t(height, vector<uint8_t>(width));
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            for (int c = 0; c < NUM_CHANNELS; ++c)
            {
                result[c][y][x] = data[(y * width + x) * NUM_CHANNELS + c];
            }
        }
    }
    stbi_image_free(data);
    return result;
}

void write_image(const string &filename, const image_t &image)
{
    int channels = image.size();
    int height = image[0].size();
    int width = image[0][0].size();

    vector<unsigned char> data(height * width * channels);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            for (int c = 0; c < channels; ++c)
            {
                data[(y * width + x) * channels + c] = image[c][y][x];
            }
        }
    }
    if (!stbi_write_png(filename.c_str(), width, height, channels, data.data(), width * channels))
    {
        throw runtime_error("Failed to write image");
    }
}


single_channel_image_t apply_box_blur(const single_channel_image_t &image, const int filter_size)
{
    // Get the dimensions of the input image
    int width = image[0].size();
    int height = image.size();
    int sum, average;


    // Create a new image to store the result
    single_channel_image_t result(height, vector<uint8_t>(width));

    // Calculate the padding size for the filter
    int pad = filter_size / 2;

    // YOUR CODE HERE

    // Loop through the image pixels, skipping the border pixels
    for(int i = pad; i < height-pad; i++){
        for(int j = pad; j < width-pad; j++){
            sum = 0;
            // Loop through the filter's rows and columns
            for(int k = -pad; k < pad+1; k++){
                for(int l = -pad; l < pad+1; l++){
                    // Add the corresponding image pixel value to the sum
                    sum = sum + image[i+k][j+l];
                }
            }
            // Calculate the average value for the current pixel
            average = sum / (filter_size * filter_size);
            // Assign the average value to the corresponding pixel in the result image
            result[i][j] = average;
        }
    }
    // Copy the border pixels from the input image to the result image
    for(int i = 0; i < height; i++){
        for(int j = 0; j < pad; j++){
            result[i][j] = image[i][j];
            result[i][width-j-1] = image[i][width-j-1];
        }
    }

    for(int j = 0; j < width; j++){
        for(int i = 0; j < pad; i++){
            result[i][j] = image[i][j];
            result[height-i-1][j] = image[height-i-1][j];
        }
    }
            
    return result;
}

int main(int argc, char *argv[])
{
    if (!filesystem::exists(INPUT_DIRECTORY))
    {
        cerr << "Error, " << INPUT_DIRECTORY << " directory does not exist" << endl;
        return 1;
    }

    if (!filesystem::exists(OUTPUT_DIRECTORY))
    {
        if (!filesystem::create_directory(OUTPUT_DIRECTORY))
        {
            cerr << "Error creating" << OUTPUT_DIRECTORY << " directory" << endl;
            return 1;
        }
    }

    if (!filesystem::is_directory(OUTPUT_DIRECTORY))
    {
        cerr << "Error there is a file named" << OUTPUT_DIRECTORY << ", it should be a directory" << endl;
        return 1;
    }

    cerr << "Error, there is a file named " << OUTPUT_DIRECTORY << ", it should be a directory";
    auto start_time = chrono::high_resolution_clock::now();
    for (auto &file : filesystem::directory_iterator{INPUT_DIRECTORY})
    {
        string input_image_path = file.path().string();
        clog << "Processing image: " << input_image_path << endl;
        image_t input_image = load_image(input_image_path);
        image_t output_image;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            output_image[i] = apply_box_blur(input_image[i], FILTER_SIZE);
        }
        string output_image_path = input_image_path.replace(input_image_path.find(INPUT_DIRECTORY), INPUT_DIRECTORY.length(), OUTPUT_DIRECTORY);
        write_image(output_image_path, output_image);
    }
    auto end_time = chrono::high_resolution_clock::now();
    auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    cout << "Elapsed time: " << elapsed_time.count() << " ms" << endl;
    return 0;
}
