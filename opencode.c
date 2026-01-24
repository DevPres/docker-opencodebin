#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DOCKER_IMAGE "ghcr.io/anomalyco/opencode"
#define DEFAULT_MOUNT "./"
#define DEFAULT_DEST "/workspace"
#define DEFAULT_OPENCODEBIN_CONFIG "~/.config/opencodebin/config.json"
#define DEFAULT_CONFIG "~/.config/opencode/"
#define DEFAULT_OPENCODE_CONFIG_DEST "/root/.config/opencode/"

typedef struct {
  char *mount_path;
  char *dest_path;
  char *work_path;
  char *env_file;
} Config;

void error_exit(const char *message) {
  fprintf(stderr, "Error: %s\n", message);
  exit(EXIT_FAILURE);
}

void print_usage(void) {
  printf("Usage: opencode [OPTIONS]\n");
  printf("Options:\n");
  printf("  -m PATH    Mount path (default: %s)\n", DEFAULT_MOUNT);
  printf("  -d PATH    Destination path (default: %s)\n", DEFAULT_DEST);
  printf("  -w PATH    Working directory (default: same as -d)\n");
  printf("  -e PATH    Environment file path\n");
  printf("  -h         Show this help message\n");
  printf("\nExample:\n");
  printf("  opencode -m /path/to/folder -d /project -w /project -e ~/.env\n");
}

bool create_path_recursive(const char *path) {
  char command[PATH_MAX + 10];
  sprintf(command, "mkdir -p \"%s\"", path);

  int result = system(command);
  return (result == 0);
}
bool validate_and_create_path(const char *path) {
  if (access(path, F_OK) == 0) {
    return true;
  }

  printf("Path '%s' does not exist. Create it? (y/n): ", path);
  char response;
  if (scanf(" %c", &response) != 1) {
    error_exit("Failed to read user input");
  }

  if (response == 'y' || response == 'Y') {
    if (!create_path_recursive(path)) {
      error_exit("Failed to create path");
    }
    printf("Path created successfully.\n");
    return true;
  }

  return false;
}
void parse_arguments(int argc, char *argv[], Config *config) {
  // Set defaults
  config->mount_path = strdup(DEFAULT_MOUNT);
  config->dest_path = strdup(DEFAULT_DEST);
  config->work_path = NULL;
  config->env_file = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "m:d:w:e:h")) != -1) {
    switch (opt) {
    case 'm':
      free(config->mount_path);
      config->mount_path = strdup(optarg);
      break;
    case 'd':
      free(config->dest_path);
      config->dest_path = strdup(optarg);
      break;
    case 'w':
      free(config->work_path);
      config->work_path = strdup(optarg);
      break;
    case 'e':
      free(config->env_file);
      config->env_file = strdup(optarg);
      break;
    case 'h':
      print_usage();
      exit(EXIT_SUCCESS);
    default:
      print_usage();
      error_exit("Invalid option");
    }
  }

  // Set working directory default if not specified
  if (config->work_path == NULL) {
    config->work_path = strdup(config->dest_path);
  }
}
char *build_docker_command(const Config *config) {
  // Calculate required buffer size
  size_t size = strlen("docker run -it -v \"") + strlen(config->mount_path) +
                1 +                                                // ":"
                strlen(config->dest_path) + 1 +                    // "\""
                strlen(" -w \"") + strlen(config->work_path) + 2 + // "\" "
                strlen(DOCKER_IMAGE) + 1;                          // "\0"

  if (config->env_file != NULL) {
    size += strlen(" --env-file \"") + strlen(config->env_file) + 1; // "\""
  }

  char *command = malloc(size);
  if (command == NULL) {
    error_exit("Memory allocation failed");
  }

  // Build command string
  if (config->env_file != NULL) {
    sprintf(command,
            "docker run -it -v \"%s:%s\" -w \"%s\" --env-file \"%s\" %s",
            config->mount_path, config->dest_path, config->work_path,
            config->env_file, DOCKER_IMAGE);
  } else {
    sprintf(command, "docker run -it -v \"%s:%s\" -w \"%s\" %s",
            config->mount_path, config->dest_path, config->work_path,
            DOCKER_IMAGE);
  }

  return command;
}
void execute_command(const char *command) {
  printf("Executing: %s\n", command);

  int result = system(command);
  if (result != 0) {
    error_exit("Docker command failed");
  }
}
void cleanup_config(Config *config) {
  free(config->mount_path);
  free(config->dest_path);
  free(config->work_path);
  free(config->env_file);

  config->mount_path = NULL;
  config->dest_path = NULL;
  config->work_path = NULL;
  config->env_file = NULL;
}
int main(int argc, char *argv[]) {
  Config config;

  // Parse command-line arguments
  parse_arguments(argc, argv, &config);

  // Validate mount path
  if (!validate_and_create_path(config.mount_path)) {
    error_exit("Mount path validation failed");
  }

  // Validate env file if provided
  if (config.env_file != NULL) {
    if (access(config.env_file, F_OK) != 0) {
      printf("%s dosen't exist\n", config.env_file);
      exit(0);
    }
  }

  // Build and execute Docker command
  char *command = build_docker_command(&config);
  execute_command(command);
  //  printf("command: %s\n", command);

  // Cleanup
  free(command);
  cleanup_config(&config);

  return 0;
}
