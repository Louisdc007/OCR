#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int input_count = 2;
int hidden_count = 4;
int sample_count = 4;
int max_epochs = 50000;
int log_interval = 200;
double learning_rate = 0.7;
double target_error = 0.05;
unsigned int random_seed = 42u;

/* Training examples */
double training_inputs[4][2] = {
    {0.0, 0.0},
    {0.0, 1.0},
    {1.0, 0.0},
    {1.0, 1.0}
};

double training_targets[4] = {
    1.0, 0.0, 0.0, 1.0
};

struct Network
{
    double input_weights[4][2];
    double hidden_biases[4];
    double output_weights[4];
    double output_bias;
};

struct Evaluation
{
    double loss;
    double max_error;
    int correct;
};

/* Transform a number into a value between 0 and 1.*/
double sigmoid(double value)
{
    if (value >= 0.0)
        return 1.0 / (1.0 + exp(-value));

    double exponential = exp(value);
    return exponential / (1.0 + exponential);
}

/* Measures the sensitivity of the neuron*/
double sigmoid_derivative(double activation)
{
    return activation * (1.0 - activation);
}

/* Return a starting value between -1 and 1. */
double random_weight(void)
{
    return 2.0 * (double)rand() / (double)RAND_MAX - 1.0;
}

/* Different starting weights prevent hidden neurons from staying equal. */
void initialize_network(struct Network *network)
{
    for (int h = 0; h < hidden_count; ++h)
    {
        for (int i = 0; i < input_count; ++i)
            network->input_weights[h][i] = random_weight();

        network->hidden_biases[h] = 0.0;
        network->output_weights[h] = random_weight();
    }
    network->output_bias = 0.0;
}

/* Predict using only the inputs and parameters*/
double predict(struct Network *network, double input[2], double hidden[4])
{
    for (int h = 0; h < hidden_count; ++h)
    {
        double sum = network->hidden_biases[h];
        for (int i = 0; i < input_count; ++i)
            sum += network->input_weights[h][i] * input[i];
        hidden[h] = sigmoid(sum);
    }

    double sum = network->output_bias;
    for (int h = 0; h < hidden_count; ++h)
        sum += network->output_weights[h] * hidden[h];

    return sigmoid(sum);
}

/* Calls predict(), measures the error, and calculates parameter updates. */
void train_sample(struct Network *network, double input[2], double target)
{
    double hidden[4];
    double hidden_delta[4];
    double output = predict(network, input, hidden);
    double output_delta = (output - target) * sigmoid_derivative(output);

    /* Compute all deltas before changing any weights. */
    for (int h = 0; h < hidden_count; ++h)
    {
        hidden_delta[h] = output_delta * network->output_weights[h] * sigmoid_derivative(hidden[h]);
    }

    /* Update the output layer: parameter -= learning_rate * gradient. */
    for (int h = 0; h < hidden_count; ++h)
    {
        network->output_weights[h] -= learning_rate * output_delta * hidden[h];
    }
    network->output_bias -= learning_rate * output_delta;

    /* Update the hidden layer using the previously computed deltas. */
    for (int h = 0; h < hidden_count; ++h)
    {
        for (int i = 0; i < input_count; ++i)
        {
            network->input_weights[h][i] -= learning_rate * hidden_delta[h] * input[i];
        }
        network->hidden_biases[h] -= learning_rate * hidden_delta[h];
    }
}

/* Measure all four cases without changing the network. Check performance */
struct Evaluation evaluate(struct Network *network)
{
    struct Evaluation result = {0.0, 0.0, 0};

    for (int sample = 0; sample < sample_count; ++sample)
    {
        double hidden[4];
        double output = predict(network, training_inputs[sample], hidden);
        double error = output - training_targets[sample];
        if (!isfinite(output) || !isfinite(error))
        {
            result.loss = INFINITY;
            result.max_error = INFINITY;
            result.correct = 0;
            return result;
        }

        result.loss += 0.5 * error * error;
        if (fabs(error) > result.max_error)
            result.max_error = fabs(error);

        int predicted = output >= 0.5;
        if (predicted == (int)training_targets[sample])
            ++result.correct;
    }

    result.loss /= sample_count;
    return result;
}

void print_parameters(struct Network *network)
{
    for (int neuron = 0; neuron < hidden_count; ++neuron)
    {
        printf("    Hidden %d | wA % .6f | wB % .6f | bias % .6f | wOut % .6f\n", neuron + 1, network->input_weights[neuron][0], network->input_weights[neuron][1], network->hidden_biases[neuron], network->output_weights[neuron]);
    }
    printf("Output bias: % .6f\n", network->output_bias);
}

/* An epoch visits all examples once, in a fixed order for simplicity. */
int train(struct Network *network)
{
    for (int epoch = 1; epoch <= max_epochs; ++epoch)
    {
        for (int sample = 0; sample < sample_count; ++sample)
        {
            train_sample(network, training_inputs[sample], training_targets[sample]);
        }

        struct Evaluation result = evaluate(network);
        if (!isfinite(result.loss) || !isfinite(result.max_error))
        {
            printf("Training failed at epoch %d: non-finite evaluation.\n", epoch);
            return 0;
        }

        int success = result.correct == sample_count && result.max_error < target_error;
        if (epoch == 1 || epoch % log_interval == 0 || success || epoch == max_epochs)
        {
            printf("Epoch %5d | loss %.6f | max error %.6f\n", epoch, result.loss, result.max_error);
            print_parameters(network);
        }

        /* Require correct classes and outputs close to every target. */
        if (success)
            return epoch;
    }

    printf("Target not reached within %d epochs.\n", max_epochs);
    return 0;
}

/* Display results*/
void print_predictions(struct Network *network)
{
    puts("A B | Target | Output   | Predicted");
    for (int sample = 0; sample < sample_count; ++sample)
    {
        double hidden[4];
        double output = predict(network, training_inputs[sample], hidden);
        printf("%.0f %.0f |   %.0f    | %.6f | %d\n", training_inputs[sample][0], training_inputs[sample][1], training_targets[sample], output, output >= 0.5);
    }

    struct Evaluation result = evaluate(network);
    printf("Correct: %d/%d | loss %.6f | max error %.6f\n", result.correct, sample_count, result.loss, result.max_error);
}

int main(void)
{
    if (input_count != 2 || hidden_count != 4 || sample_count != 4)
    {
        printf("Array sizes are fixed: 2 inputs, 4 hidden neurons, 4 samples.\n");
        return 1;
    }

    struct Network network;

    srand(random_seed);//to get the same "random" number
    initialize_network(&network);

    puts("Before training:");
    print_predictions(&network);
    puts("\nTraining:");

    int epochs = train(&network);
    puts("\nAfter training:");
    print_predictions(&network);

    if (epochs == 0){
        return 1;
    }
    printf("\nTraining completed after %d epochs.\n", epochs);
    return EXIT_SUCCESS;
}
