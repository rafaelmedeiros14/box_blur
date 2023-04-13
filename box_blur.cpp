// Aluno: Rafael Araujo Medeiros    Matricula: 2019021697
#include <iostream>
#include <condition_variable>
#include <vector>
#include <mutex>
#include <array>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cassert>

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

// Variavel para guardar a quantidade de imagem no diretorio
int file_count = 0;

// Mutex para proteger os recursos compartilhados
std::mutex m;
// Variavel de condicao que indica que existe espaco disponivel no buffer
// O consumidor utiliza essa variavel de condicao para notificar o produtor que a fila nao esta cheia
std::condition_variable space_available;
// Variavel de condicao que indica que existem dados disponiveis no buffer
// O produtor utiliza essa variavel de condicao para notificar o consumidor que a fila nao esta vazia
std::condition_variable data_available;

// Numero de threads produtoras e consumidoras a serem criadas no main()
static const unsigned NUM_PRODUCERS = 1;
static const unsigned NUM_CONSUMERS = 10;

//  =========================  Circular buffer  ============================================
// Codigo que implementa um buffer circular
static const unsigned BUFFER_SIZE = 1000;
string buffer[BUFFER_SIZE];
static unsigned counter = 0;
unsigned in = 0, out = 0;

void add_buffer(string i)
{
  buffer[in] = i;
  in = (in+1) % BUFFER_SIZE;
  counter++;
}

string get_buffer()
{
  string v;
  v = buffer[out];
  out = (out+1) % BUFFER_SIZE;
  counter--;
  return v;
}

// Altere esse valor caso queira que o produtor e consumidor durmam SLEEP_TIME milissegundos
// apos cada iteracao 
static const unsigned SLEEP_TIME = 0; // ms

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
        for(int i = 0; i < pad; i++){
            result[i][j] = image[i][j];
            result[height-i-1][j] = image[height-i-1][j];
        }
    }
            
    return result;
}

// Producer
void producer_func(const unsigned id)
{
	int i = 0;
	for (auto &file : filesystem::directory_iterator{INPUT_DIRECTORY})
    {
		// Cria um objeto do tipo unique_lock que no construtor chama m.lock()
		std::unique_lock<std::mutex> lock(m);

		// Verifica se o buffer esta cheio e, caso afirmativo, espera notificacao de "espaco disponivel no buffer"
		while (counter == BUFFER_SIZE)
		{			
			space_available.wait(lock); // Espera por espaco disponivel 
			// Lembre-se que a funcao wait() invoca m.unlock() antes de colocar a thread em estado de espera para que o consumidor consiga adquirir a posse do mutex m e consumir dados
			// Quando a thread eh acordada, a funcao wait() invoca m.lock() retomando a posse do mutex m
		}

        string input_image_path = file.path().string();

		// O buffer nao esta mais cheio, entao produz um elemento
		add_buffer(input_image_path);
		
        cout << "Producer " << id << " - produced: " << i << " - Buffer counter: " << counter << endl;
        i++;

		// Notifica o consumiror que existem dados a serem consumidos no buffer
		data_available.notify_one();
		
		// (Opicional) dorme por SLEEP_TIME milissegundos
		if (SLEEP_TIME > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));

		// Verifica a integridade do buffer, ou seja, verifica se o numero de elementos do buffer (counter)
		// eh menor ou igual a BUFFER_SIZE
		assert(counter <= BUFFER_SIZE);		
	} // Fim de escopo -> o objeto lock sera destruido e invocara m.unlock(), liberando o mutex m
}

// Consumer
void consumer_func(const unsigned id)
{
    int i = 0;
	while (i < file_count)
    {
		// Cria um objeto do tipo unique_lock que no construtor chama m.lock()
		std::unique_lock<std::mutex> lock(m);
		
		// Verifica se o buffer esta vazio e, caso afirmativo, espera notificacao de "dado disponivel no buffer"
		while (counter == 0)
		{
			data_available.wait(lock); // Espera por dado disponivel
			// Lembre-se que a funcao wait() invoca m.unlock() antes de colocar a thread em estado de espera para que o produtor consiga adquirir a posse do mutex m e produzir dados
			// Quando a thread eh acordada, a funcao wait() invoca m.lock() retomando a posse do mutex m
		}
        
		// O buffer nao esta mais vazio, entao consome um elemento
		string input_image_path = get_buffer();
		
        clog << "Processing image: " << input_image_path << endl;
        image_t input_image = load_image(input_image_path);
        image_t output_image;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            output_image[i] = apply_box_blur(input_image[i], FILTER_SIZE);
        }
        string output_image_path = input_image_path.replace(input_image_path.find(INPUT_DIRECTORY), INPUT_DIRECTORY.length(), OUTPUT_DIRECTORY);
        write_image(output_image_path, output_image);

        cout << "Consumer " << id << " - Buffer counter: " << counter << endl;

		space_available.notify_one();
		
		// (Opicional) dorme por SLEEP_TIME milissegundo
		if (SLEEP_TIME > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));

		// Verifica a integridade do buffer, ou seja, verifica se o numero de elementos do buffer (counter)
		// eh maior ou igual a zero
		assert(counter >= 0);
	}  // Fim de escopo -> o objeto lock sera destruido e invocara m.unlock(), liberando o mutex m
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
        cerr << "Error there is a file named " << OUTPUT_DIRECTORY << ", it should be a directory" << endl;
        return 1;
    }
    
    for (auto &file : filesystem::directory_iterator{INPUT_DIRECTORY}){
        file_count++;
    }
    // Cria NUM_PRODUCER thread produtoras  e NUM_CONSUMER threads consumidoras
	std::vector<std::thread> producers;
	std::vector<std::thread> consumers;

	for (unsigned i =0; i < NUM_PRODUCERS; ++i)
	{
		producers.push_back(std::thread(producer_func, i));
	}
	for (unsigned i =0; i < NUM_CONSUMERS; ++i)
	{
		consumers.push_back(std::thread(consumer_func, i));
	}

    consumers[0].join();
    
    return 0;
}
