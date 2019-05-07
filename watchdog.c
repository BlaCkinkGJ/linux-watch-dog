#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <glib/glist.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define INFINITY_RUN() while (1)
#define update_every_10s(det) det ^= true;

GList *file_list;
char path[PATH_MAX];
char base_dir[PATH_MAX];

struct file_info {
  char path[PATH_MAX];
  struct stat status;
};

void task_handler(int);
void destructor_handler(int);

void initializer(struct file_info *);

void modify_detect(struct file_info *);
void create_detect(struct file_info *);

void traverse(const char *, void (*)(struct file_info *));

int main(void) {
  signal(SIGALRM, task_handler);
  signal(SIGINT, destructor_handler);

  sprintf(base_dir, "%s/workspace", getenv("HOME"));
  traverse(base_dir, initializer);

  alarm(5);
  INFINITY_RUN();
  return 0;
}

void task_handler(int signo) {
  static time_t acc_time = 0;
  acc_time += 5;
  alarm(5);
  static bool period_reach = false;
  if (period_reach) {
    printf("[start+%ld(%s)] creation and modification check...\n", acc_time,
           base_dir);
    traverse(base_dir, modify_detect);
  } else {
    printf("[start+%ld(%s)] creation check...\n", acc_time, base_dir);
  }
  traverse(base_dir, create_detect);
  update_every_10s(period_reach);
}

void destructor_handler(int signo) {
  printf("\n\nINTERRUPT SIGNAL CATCH\n\n");
  g_list_foreach(file_list, (GFunc)g_free, NULL);
  g_list_free_1(file_list);
  exit(EXIT_SUCCESS);
}

void initializer(struct file_info *info) {
  file_list = g_list_append(file_list, info);
}

static bool compare_stat(const struct stat *src, const struct stat *dst) {
  return !((src->st_uid == dst->st_uid)          // user id
           && (src->st_gid == dst->st_gid)       // group id
           && (src->st_size == dst->st_size)     // file size
           && (src->st_atime == dst->st_atime)   // last access time
           && (src->st_mtime == dst->st_mtime)); // modification time
}

void modify_detect(struct file_info *check_entry) {
  GList *it;
  for (it = file_list; it != NULL; it = it->next) {
    struct file_info *base_entry = it->data;
    // find the exactly same directory
    if (base_entry->status.st_ino == check_entry->status.st_ino) {
      // file and directory name change check
      bool is_diff = strcmp(base_entry->path, check_entry->path);
      // status change check
      is_diff |= compare_stat(&(base_entry->status), &(check_entry->status));
      if (is_diff) {
        printf("\n[[ MODIFICATION ALERT ]]\n\n");
        printf("PREVIOUS FILE\t==>%s\nsize: %ld\taccess: %ld\tmodify: "
               "%ld\nuid: %d\tgid: %d\n",
               base_entry->path, base_entry->status.st_size,
               base_entry->status.st_atime, base_entry->status.st_mtime,
               base_entry->status.st_uid, base_entry->status.st_gid);
        printf("CURRENT FILE\t==>%s\nsize: %ld\taccess: %ld\tmodify: %ld\nuid: "
               "%d\tgid: %d\n\n",
               check_entry->path, check_entry->status.st_size,
               check_entry->status.st_atime, check_entry->status.st_mtime,
               check_entry->status.st_uid, check_entry->status.st_gid);
      }
    } // end of if
  }   // end of for
}

void create_detect(struct file_info *check_entry) {
  GList *it;
  for (it = file_list; it != NULL; it = it->next) {
    struct file_info *base_entry = it->data;
    if (base_entry->status.st_ino == check_entry->status.st_ino) {
      return;
    }
  }
  printf("\n[[ UNAUTHORIZED FILE CREATION DETECTION ]]\n\n");
  printf("FILE NAME\t==>%s\n", check_entry->path);
  printf("REMOVE SEQUENCE OPERATION\n");
  if (S_ISDIR(check_entry->status.st_mode)) {
    if (rmdir(check_entry->path)) {
      fprintf(stderr, "Remove directory failed\n");
      exit(EXIT_FAILURE);
    } // end of if
  } else {
    if (remove(check_entry->path)) {
      fprintf(stderr, "Remove file failed\n");
      exit(EXIT_FAILURE);
    } // end of if
  }   // end of if
}

void traverse(const char *dir_name, void (*func)(struct file_info *)) {
  int path_length = strlen(path);
  DIR *cur_dir;
  struct dirent *entry;

  if (strlen(path) + strlen(dir_name) > PATH_MAX) {
    fprintf(stderr, "Max path size arrives\n");
    exit(EXIT_FAILURE);
  }

  strcat(path, dir_name);
  strcat(path, "/");

  cur_dir = opendir(path);
  if (cur_dir == NULL) {
    fprintf(stderr, "Cannot open directory '%s': %s\n", dir_name,
            strerror(errno));
    exit(EXIT_FAILURE);
  }

  while ((entry = readdir(cur_dir)) != NULL) {
    bool is_period =
        !(strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."));
    if (!is_period) {
      char abs_path[PATH_MAX];
      struct file_info *info = NULL;

      // make absolute path
      sprintf(abs_path, "%s%s", path, entry->d_name);

      // get file status
      info = (struct file_info *)malloc(sizeof(struct file_info));
      stat(abs_path, &(info->status));
      strcpy(info->path, abs_path);

      // traverse the subdirectory
      if (S_ISDIR(info->status.st_mode)) {
        traverse(entry->d_name, func);
      }
      // do what I have to do
      (*func)(info);
    } // end of if
  }

  if (closedir(cur_dir)) {
    fprintf(stderr, "Could not close '%s': %s\n", dir_name, strerror(errno));
    exit(EXIT_FAILURE);
  }

  path[path_length] = '\0';
}