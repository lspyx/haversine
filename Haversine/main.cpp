#include "common.h"
#include "json_parser.h"
#include <sys/stat.h>
#include <iomanip>
namespace lsp {

    static double Square(double a) {
        return a * a;
    }

    static double RadiansFromDegrees(double degrees) {
        return 0.01745329251994329577 * degrees;
    }

static double ReferenceHaversine(double X0, double Y0, double X1, double Y1, double EarthRadius = 6372.8) {
  double lat1 = Y0, lat2 = Y1, lon1 = X0, lon2 = X1;
  double dLat = RadiansFromDegrees(lat2 - lat1);
  double dLon = RadiansFromDegrees(lon2 - lon1);
  lat1 = RadiansFromDegrees(lat1);
  lat2 = RadiansFromDegrees(lat2);

  double a = Square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * Square(sin(dLon / 2.0));
  double c = 2.0 * asin(sqrt(a));

  double Result = EarthRadius * c;

  return Result;
}

    static buffer read_entire_file(const char *filename) {
        buffer result = {};
        FILE *file = fopen(filename, "rb");
        if (file) {
            struct stat s = {};
            stat(filename, &s);
            result = allocate_buffer(s.st_size);
            if (result.data) {
                if (fread(result.data, result.count, 1, file) != 1) {
                    fprintf(stderr, "Error: unable to read `%s`.\n", filename);
                    free_buffer(&result);
                }
            }
            fclose(file);
        } else {
            fprintf(stderr, "Error: unable to open `%s`.\n", filename);
        }
        return result;
    }

    static double sum_haversine_distances(uint64_t pair_count, haversine_pair *pairs) {
        double sum = 0;
        for (uint64_t i = 0; i < pair_count; i++) {
            haversine_pair pair = pairs[i];
            constexpr double EARTH_RADIUS = 6372.8;
            double dist = ReferenceHaversine(pair.x0,
                                             pair.y0,
                                             pair.x1,
                                             pair.y1,
                                             EARTH_RADIUS);
            //            std::cout << pair.x0 << " "
            //                      << pair.y0 << " "
            //                      << pair.x1 << " "
            //                      << pair.y1 << " "
            //                      << dist << std::endl;
            sum += (dist / pair_count);
        }
        return sum;
    }

    
}
int main(int argc, char **argv)
{
    int result = 1;
    if((argc == 2) || (argc == 3))
    {
        buffer input_json = lsp::read_entire_file(argv[1]);
        unsigned min_json_pair_encoding = 6*4;
        uint64_t max_pair_count = input_json.count / min_json_pair_encoding;
        if(max_pair_count)
        {
            buffer parsed_values = allocate_buffer(max_pair_count * sizeof(haversine_pair));
            if(parsed_values.count)
            {
                haversine_pair *pairs = (haversine_pair *)parsed_values.data;
                uint64_t pair_count = json::parse_haversine_pairs(input_json, max_pair_count, pairs);
                double sum = lsp::sum_haversine_distances(pair_count, pairs);
                
                fprintf(stdout, "Input size: %llu\n", input_json.count);
                fprintf(stdout, "Pair count: %llu\n", pair_count);
                fprintf(stdout, "Haversine sum: %.16f\n", sum);
                
                if(argc == 3)
                {
                    buffer answers_double = lsp::read_entire_file(argv[2]);
                    if(answers_double.count >= sizeof(double))
                    {
                        double *answer_values = (double *)answers_double.data;
                        
                        fprintf(stdout, "\nValidation:\n");
                        
                        uint64_t ref_answer_count = (answers_double.count - sizeof(double)) / sizeof(double);
                        if(pair_count != ref_answer_count)
                        {
                            fprintf(stdout, "FAILED - pair count doesn't match %llu.\n", ref_answer_count);
                        }
                        
                        double ref_sum = answer_values[ref_answer_count];
                        fprintf(stdout, "Reference sum: %.16f\n", ref_sum);
                        fprintf(stdout, "Difference: %.16f\n", sum - ref_sum);
                        
                        fprintf(stdout, "\n");
                    }
                }
            }
            
            free_buffer(&parsed_values);
        }
        else
        {
            fprintf(stderr, "ERROR: Malformed input JSON\n");
        }

        free_buffer(&input_json);
        
        result = 0;
    }
    else
    {
        fprintf(stderr, "Usage: %s [haversine_input.json]\n", argv[0]);
        fprintf(stderr, "       %s [haversine_input.json] [answers.double]\n", argv[0]);
    }
    
    return result;
}
