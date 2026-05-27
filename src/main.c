#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "output.h"
#include "parser.h"

static int process_stream(FILE *stream, const canframe_cli_options_t *options) {
  char line[512];

  while (fgets(line, sizeof(line), stream) != NULL) {
    can_frame_t frame;
    canframe_status_t status;
    size_t len = strlen(line);

    if (len > 0 && line[len - 1U] == '\n') {
      line[len - 1U] = '\0';
    }
    if (line[0] == '\0') {
      continue;
    }

    status = parse_line(line, &frame);
    if (status != CANFRAME_OK) {
      fprintf(stderr, "canframe: %s: %s\n", canframe_status_string(status), line);
      return 1;
    }

    if (!canframe_filter_matches(options, frame.id)) {
      continue;
    }

    if (options->json_mode) {
      output_json(stdout, &frame);
    } else {
      output_raw(stdout, &frame);
    }
  }

  return 0;
}

int main(int argc, char **argv) {
  canframe_cli_options_t options;
  int parse_result = canframe_parse_args(argc, argv, &options);

  if (parse_result == 1) {
    canframe_print_usage(argv[0]);
    return 0;
  }
  if (parse_result != 0) {
    canframe_print_usage(argv[0]);
    return 1;
  }

  if (options.inline_frame != NULL) {
    can_frame_t frame;
    canframe_status_t status = parse_line(options.inline_frame, &frame);
    if (status != CANFRAME_OK) {
      fprintf(stderr, "canframe: %s\n", canframe_status_string(status));
      return 1;
    }
    if (canframe_filter_matches(&options, frame.id)) {
      if (options.json_mode) {
        output_json(stdout, &frame);
      } else {
        output_raw(stdout, &frame);
      }
    }
    return 0;
  }

  if (options.input_path != NULL) {
    FILE *input = fopen(options.input_path, "r");
    int result;
    if (input == NULL) {
      perror("canframe");
      return 1;
    }
    result = process_stream(input, &options);
    fclose(input);
    return result;
  }

  return process_stream(stdin, &options);
}

