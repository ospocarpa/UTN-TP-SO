#include "kernel.h"

const char* CONFIG_FIELDS[] = { PUERTO_PROG, PUERTO_CPU,
IP_MEMORIA, PUERTO_MEMORIA, IP_FS, PUERTO_FS, QUANTUM,
QUANTUM_SLEEP, ALGORITMO, GRADO_MULTIPROG, SEM_IDS, SEM_INIT,
SHARED_VARS, STACK_SIZE };

bool is_cpu_free() {
	log_debug(logger, "is_cpu_free");
	pthread_mutex_lock(&cpu_mutex);
	bool find(void* e) {
		t_cpu* cpu = e;
		return !cpu->busy;
	}

	bool result = list_any_satisfy(cpu_list, find);
	pthread_mutex_unlock(&cpu_mutex);
	return result;
}

t_cpu* get_cpu_free() {
	log_debug(logger, "get_cpu_free");
	pthread_mutex_lock(&cpu_mutex);
	bool find(void* e) {
		t_cpu* cpu = e;
		return !cpu->busy;
	}

	t_cpu* result = list_find(cpu_list, find);
	pthread_mutex_unlock(&cpu_mutex);
	return result;
}

void short_planning() {
	log_debug(logger, "short_planning");
	kill_pending_process();
	pthread_mutex_lock(&planning_mutex);
	if (planning_running && is_cpu_free() && list_size(ready_list) > 0) {
		t_cpu* free_cpu = get_cpu_free();
		pcb* _pcb = list_get(ready_list, 0);

		move_to_list(_pcb, EXEC_LIST);
		free_cpu->busy = true;
		free_cpu->xpid = _pcb->pid;

		pthread_mutex_lock(&json_mutex);
		char* pcb_string = pcb_to_string(_pcb);
		pthread_mutex_unlock(&json_mutex);

		config_destroy(config);
		config = malloc(sizeof(t_config));
		config = config_create(path_config);

		quantum_sleep = config_get_int_value(config, QUANTUM_SLEEP);

		runFunction(free_cpu->socket, "kernel_receive_pcb", 4, string_itoa(planning_alg), string_itoa(quantum), pcb_string, string_itoa(quantum_sleep));
		free(pcb_string);
		pthread_mutex_unlock(&planning_mutex);
		short_planning();
		return;
	}
	pthread_mutex_unlock(&planning_mutex);
}

int find_pcb_pos_in_list(t_list* list, int pid) {
	log_debug(logger, "find_pcb_pos_in_list");
	int i;
	for (i = 0; i < list_size(list); i++) {
		pcb* f_pcb = list_get(list, i);
		log_debug(logger, "find_pcb_pos_in_list: f_pcb->pid: '%d', pid: '%d'", f_pcb->pid, pid);
		if (f_pcb->pid == pid)
			return i;
	}
	log_debug(logger, "find_pcb_pos_in_list: return -1");

	return -1;
}

void remove_cpu_from_cpu_list(t_cpu* cpu) {
	log_debug(logger, "remove_cpu_from_cpu_list");
	pthread_mutex_lock(&cpu_mutex);
	int i;
	for (i = 0; i < list_size(cpu_list); i++) {
		t_cpu* n_cpu = list_get(cpu_list, i);
		if (n_cpu->socket == cpu->socket) {
			list_remove(cpu_list, i);
			break;
		}
	}
	pthread_mutex_unlock(&cpu_mutex);
}

t_cpu* find_cpu_by_xpid(int pid) {
	log_debug(logger, "find_cpu_by_xpid");
	pthread_mutex_lock(&cpu_mutex);
	bool find(void* element) {
		t_cpu* cpu = element;
		return cpu->xpid == pid;
	}

	t_cpu* cpu = list_find(cpu_list, &find);
	pthread_mutex_unlock(&cpu_mutex);

	return cpu;
}

t_cpu* find_cpu_by_socket(int socket) {
	log_debug(logger, "find_cpu_by_socket");
	pthread_mutex_lock(&cpu_mutex);
	bool find(void* element) {
		t_cpu* cpu = element;
		return cpu->socket == socket;
	}

	t_cpu* cpu = list_find(cpu_list, &find);
	pthread_mutex_unlock(&cpu_mutex);

	return cpu;
}

pcb* find_pcb_by_pid(int pid) {
	log_debug(logger, "find_pcb_by_pid");

	pthread_mutex_lock(&pcb_list_mutex);
	bool find(void* element) {
		t_socket_pcb* n_pcb = element;
		return n_pcb->pid == pid;
	}

	t_socket_pcb* socket_pcb = list_find(socket_pcb_list, &find);

	if (socket_pcb == NULL) {
		log_debug(logger, "find_pcb_by_pid: socket_pcb: NULL");
		pthread_mutex_unlock(&pcb_list_mutex);
		return NULL;
	}

	int pos;
	pcb* n_pcb;// = malloc(sizeof(pcb));
	switch (socket_pcb->state) {
		case NEW_LIST:
			pos = find_pcb_pos_in_list(new_queue->elements, socket_pcb->pid);
			n_pcb = list_get(new_queue->elements, pos);
			break;
		case READY_LIST:
			pos = find_pcb_pos_in_list(ready_list, socket_pcb->pid);
			n_pcb = list_get(ready_list, pos);
			break;
		case EXEC_LIST:
			pos = find_pcb_pos_in_list(exec_list, socket_pcb->pid);
			n_pcb = list_get(exec_list, pos);
			break;
		case BLOCK_LIST:
			pos = find_pcb_pos_in_list(block_list, socket_pcb->pid);
			n_pcb = list_get(block_list, pos);
			break;
		case EXIT_LIST:
			pos = find_pcb_pos_in_list(exit_queue->elements, socket_pcb->pid);
			n_pcb = list_get(exit_queue->elements, pos);
			break;
	}
	pthread_mutex_unlock(&pcb_list_mutex);

	return n_pcb;
}

pcb* find_pcb_by_pid2(int pid) {
	log_debug(logger, "find_pcb_by_pid2");

	pthread_mutex_lock(&pcb_list_mutex);
	bool find(void* element) {
		t_socket_pcb* n_pcb = element;
		return n_pcb->pid == pid;
	}

	t_socket_pcb* socket_pcb = list_find(socket_pcb_list, &find);

	if (socket_pcb == NULL) {
		log_debug(logger, "find_pcb_by_pid: socket_pcb: NULL");
		pthread_mutex_unlock(&pcb_list_mutex);
		return NULL;
	}

	int pos;
	pcb* n_pcb;// = malloc(sizeof(pcb));
	switch (socket_pcb->state) {
		case NEW_LIST:
			pos = find_pcb_pos_in_list(new_queue->elements, socket_pcb->pid);
			n_pcb = list_get(new_queue->elements, pos);
			break;
		case READY_LIST:
			pos = find_pcb_pos_in_list(ready_list, socket_pcb->pid);
			n_pcb = list_get(ready_list, pos);
			break;
		case EXEC_LIST:
			pos = find_pcb_pos_in_list(exec_list, socket_pcb->pid);
			n_pcb = list_get(exec_list, pos);
			break;
		case BLOCK_LIST:
			pos = find_pcb_pos_in_list(block_list, socket_pcb->pid);
			n_pcb = list_get(block_list, pos);
			break;
		case EXIT_LIST:
			pos = find_pcb_pos_in_list(exit_queue->elements, socket_pcb->pid);
			n_pcb = list_get(exit_queue->elements, pos);
			break;
	}
	pthread_mutex_unlock(&pcb_list_mutex);

	return n_pcb;
}

pcb* find_pcb_by_socket(int socket) {
	log_debug(logger, "find_pcb_by_socket");
	pthread_mutex_lock(&pcb_list_mutex);
	bool find(void* element) {
		t_socket_pcb* n_pcb = element;
		return n_pcb->socket == socket;
	}

	t_socket_pcb* socket_pcb = list_find(socket_pcb_list, &find);
	if (socket_pcb != NULL) {
		int pos;
		pcb* n_pcb;// = malloc(sizeof(pcb));
		switch (socket_pcb->state) {
			case NEW_LIST:
				pos = find_pcb_pos_in_list(new_queue->elements, socket_pcb->pid);
				n_pcb = list_get(new_queue->elements, pos);
				break;
			case READY_LIST:
				pos = find_pcb_pos_in_list(ready_list, socket_pcb->pid);
				n_pcb = list_get(ready_list, pos);
				break;
			case EXEC_LIST:
				pos = find_pcb_pos_in_list(exec_list, socket_pcb->pid);
				n_pcb = list_get(exec_list, pos);
				break;
			case BLOCK_LIST:
				pos = find_pcb_pos_in_list(block_list, socket_pcb->pid);
				n_pcb = list_get(block_list, pos);
				break;
			case EXIT_LIST:
				pos = find_pcb_pos_in_list(exit_queue->elements, socket_pcb->pid);
				n_pcb = list_get(exit_queue->elements, pos);
				break;
		}
		pthread_mutex_unlock(&pcb_list_mutex);

		return n_pcb;
	}
	pthread_mutex_unlock(&pcb_list_mutex);
	return NULL;
}

void substract_process_in_memory() {
	log_debug(logger, "substract_process_in_memory");
	pthread_mutex_lock(&process_in_memory_mutex);

	process_in_memory--;
	if (process_in_memory < multiprog)
		pthread_mutex_unlock(&console_mutex);

	pthread_mutex_unlock(&process_in_memory_mutex);
}

void add_process_in_memory() {
	log_debug(logger, "add_process_in_memory");
	pthread_mutex_lock(&process_in_memory_mutex);

	process_in_memory++;
	if (process_in_memory < multiprog)
		pthread_mutex_unlock(&console_mutex);

	pthread_mutex_unlock(&process_in_memory_mutex);
}

void remove_sem_pid_list(int pid) {
	int i, j;
	for(i = 0; i < list_size(sem_pid_list); i++) {
		t_sem_pid* sem_pid = list_get(sem_pid_list, i);

		if(sem_pid->pid == pid) {
			for(j = 0; j < list_size(sem_ids); j++) {
				t_sem* sem = list_get(sem_ids, j);

				if(strcmp(sem->id, sem_pid->sem) == 0 && sem->value < sem->init_value)
					sem->value++;
				if(strcmp(sem->id, sem_pid->sem) == 0) {
					int process = get_first(sem->blocked_pids);
					if(process > -1) {
						pcb* _pcb = find_pcb_by_pid(process);
						if(_pcb->state == BLOCK_LIST && sem->value >= 0) {
							char* temp = remove_first(sem->blocked_pids);
							sem->blocked_pids = string_new();
							string_append(&sem->blocked_pids, temp);
							move_to_list(_pcb, READY_LIST);
							short_planning();
						}
					}
				}
			}

			list_remove(sem_pid_list, i);
		}
	}
}

void move_to_list(pcb* pcb, int list_name) {
	bool find_pcb(void* element) {
		t_socket_pcb* p = element;
		return p->pid == pcb->pid;
	}

	pthread_mutex_lock(&pcb_list_mutex);

	int pos;

	t_socket_pcb* p = list_find(socket_pcb_list, &find_pcb);
	if(p != NULL && p->state != pcb->state)
		pcb->state = p->state;

	if (pcb->state == list_name){
		pthread_mutex_unlock(&pcb_list_mutex);
		return;
	}
	log_debug(logger, "move_to_list:pid %d from %d to %d [NEW_LIST = 1, READY_LIST = 2, EXEC_LIST = 3, BLOCK_LIST = 4, EXIT_LIST = 5", pcb->pid, pcb->state, list_name);
	switch (pcb->state) {
		case NEW_LIST:
			pos = find_pcb_pos_in_list(new_queue->elements, pcb->pid);
			list_remove(new_queue->elements, pos);
			break;
		case READY_LIST:
			pos = find_pcb_pos_in_list(ready_list, pcb->pid);
			list_remove(ready_list, pos);
			break;
		case EXEC_LIST:
			pos = find_pcb_pos_in_list(exec_list, pcb->pid);
			list_remove(exec_list, pos);
			break;
		case BLOCK_LIST:
			pos = find_pcb_pos_in_list(block_list, pcb->pid);
			list_remove(block_list, pos);
			break;
		case EXIT_LIST:
			pos = find_pcb_pos_in_list(exit_queue->elements, pcb->pid);
			list_remove(exit_queue->elements, pos);
			break;
	}

	switch (list_name) {
		case NEW_LIST:
			queue_push(new_queue, pcb);
			break;
		case READY_LIST:
			list_add(ready_list, pcb);
			break;
		case EXEC_LIST:
			list_add(exec_list, pcb);
			break;
		case BLOCK_LIST:
			list_add(block_list, pcb);
			break;
		case EXIT_LIST:
			queue_push(exit_queue, pcb);
			break;
	}
	pcb->state = list_name;

	int i;
	for (i = 0; i < list_size(socket_pcb_list); i++) {
		t_socket_pcb* socket_pcb = list_get(socket_pcb_list, i);
		if (socket_pcb->pid == pcb->pid) {
			socket_pcb->state = list_name;
			break;
		}
	}
	pthread_mutex_unlock(&pcb_list_mutex);

	if(list_name == EXIT_LIST){
		remove_sem_pid_list(pcb->pid);
	}
}

/*
 * CPU-FILESYSTEM
 */

bool validate_file_from_fs(char* path, int pid) {
	log_debug(logger, "validate_file_from_fs");
	runFunction(fs_socket, "kernel_validate_file", 2, path, string_itoa(pid));
	wait_response(&fs_mutex);
	return fs_response;
}

//GETTERS TABLA GLOBAL
int get_gfd_by_path(char* path) {
	log_debug(logger, "get_gfd_by_path");
	int i;
	for (i = 0; i < list_size(fs_global_table); i++) {
		t_global_file_table* file = list_get(fs_global_table, i);
		if (!strcmp(file->path, path))
			return file->gfd;
	}
	return -1; //No existe
}

int get_pos_in_fs_global_table_by_gfd(int gfd) {
	log_debug(logger, "get_pos_in_fs_global_table_by_gfd");
	int i;
	for (i = 0; i < list_size(fs_global_table); i++) {
		t_global_file_table* global_files = list_get(fs_global_table, i);
		if (global_files->gfd == gfd)
			return i;
	}
	return -1; //No existe
}

char* get_path_by_gfd(int gfd) {
	log_debug(logger, "get_path_by_gfd");
	int pos = get_pos_in_fs_global_table_by_gfd(gfd);
	t_global_file_table* n_file = list_get(fs_global_table, pos);
	return n_file->path;
}

t_global_file_table* get_global_file_by_gfd(int gfd) {
	log_debug(logger, "get_global_file_by_gfd");
	int i;
	for (i = 0; i < list_size(fs_global_table); i++) {
		t_global_file_table* file = list_get(fs_global_table, i);
		if (file->gfd == gfd)
			return file;
	}
	return NULL;
}

////GETTERS TABLA DE ARCHIVOS DE PROCESOS

t_process_file_table* get_process_file_by_pid( pid) {
	log_debug(logger, "get_process_file_by_pid");
	int i;
	for (i = 0; i < list_size(fs_process_table); i++) {
		t_process_file_table* file = list_get(fs_process_table, i);
		if (file->pid == pid)
			return file;
	}
	return NULL; //No existe
}

int get_pos_open_file_by_fd(t_list* open_files, int fd) {
	log_debug(logger, "get_pos_open_file_by_fd");
	int i;
	for (i = 0; i < list_size(open_files); i++) {
		t_open_file* file = list_get(open_files, i);
		if (file->fd == fd)
			return i;
	}
	return -1; //No existe
}

int get_pos_in_fs_process_table_by_pid(int pid) {
	log_debug(logger, "get_pos_in_fs_process_table_by_pid");
	int i;
	for (i = 0; i < list_size(fs_process_table); i++) {
		t_process_file_table* file = list_get(fs_process_table, i);
		if (file->pid == pid)
			return i;
	}
	return -1; //No existe File en la table
}

t_open_file* get_open_file_by_fd_and_pid(int fd, int pid) {
	log_debug(logger, "get_open_file_by_fd_and_pid");
	t_process_file_table* files_open_process = get_process_file_by_pid(pid);
	if (files_open_process == NULL)
		return NULL;
	int i;
	for (i = 0; i < list_size(files_open_process->open_files); i++) {
		t_open_file* file = list_get(files_open_process->open_files, i);
		if (file->fd == fd)
			return file;
	}
	return NULL;
}

char* get_path_by_fd_and_pid(int fd, int pid) {
	log_debug(logger, "get_path_by_fd_and_pid");
	t_open_file* process = get_open_file_by_fd_and_pid(fd, pid);
	if (process == NULL)
		return NULL;
	return get_path_by_gfd(process->gfd);
}

//ADDS

int open_file_in_process_table(char* path, char* flag, int pid) {
	log_debug(logger, "open_file_in_process_table");
	int pos = get_pos_in_fs_process_table_by_pid(pid);
	if (pos == -1) {
		t_process_file_table* n_file = malloc(sizeof(t_process_file_table));
		n_file->pid = pid;
		n_file->open_files = add_defaults_fds();
		pos = list_add(fs_process_table, n_file);
	}

	int gfd = add_file_in_global_table(path);

	t_open_file* n_file = malloc(sizeof(t_open_file));
	n_file->gfd = gfd;
	n_file->flag = string_new();
	string_append(&n_file->flag, flag);
	n_file->pointer = 0;

	t_process_file_table* files_open_process = get_process_file_by_pid(pid);

	n_file->fd = list_size(files_open_process->open_files);
	int fd = list_add(files_open_process->open_files, n_file);
	list_replace(fs_process_table, pos, files_open_process);

	return fd;
}

int add_file_in_global_table(char* path) {
	log_debug(logger, "add_file_in_global_table");
	t_global_file_table* n_file = malloc(sizeof(t_global_file_table));

	int gfd = get_gfd_by_path(path);
	if (gfd == -1) { //No existe
		n_file->path = string_new();
		string_append(&n_file->path, path);
		n_file->open = 1;
		n_file->gfd = list_size(fs_global_table);
		list_add(fs_global_table, n_file);
	} else {
		int pos = get_pos_in_fs_global_table_by_gfd(get_gfd_by_path(path));
		n_file = list_get(fs_global_table, pos);
		n_file->open++;
		list_replace(fs_global_table, pos, n_file);
	}
	return n_file->gfd;
}

int remove_open_file_by_fd_and_pid(int fd, int pid) {
	log_debug(logger, "remove_open_file_by_fd_and_pid");
	t_process_file_table* files = get_process_file_by_pid(pid);
	if (files == NULL)
		return ARCHIVO_SIN_ABRIR_PREVIAMENTE; //El proceso no abrio nunca un archivo

	int pos = get_pos_open_file_by_fd(files->open_files, fd);
	if (pos == -1)
		return ARCHIVO_SIN_ABRIR_PREVIAMENTE; // El proceso no tiene ese fd asignado

	t_open_file* file_to_remove = list_get(files->open_files, pos);
	int gfd = file_to_remove->gfd;
	list_remove_and_destroy_element(files->open_files, pos, &free);
	pos = get_pos_in_fs_process_table_by_pid(pid);

	list_replace(fs_process_table, pos, files);

	if (list_size(files->open_files) == 3)
		list_remove_and_destroy_element(fs_process_table, get_pos_in_fs_process_table_by_pid(pid), &free); //Si no tiene archivos abierto la remuevo de la lista

	return gfd;
}

int close_file(int fd_close, int pid) {
	log_debug(logger, "close_file");
	if (is_default(fd_close))
		return NO_EXISTE_ARCHIVO;

	int gfd = remove_open_file_by_fd_and_pid(fd_close, pid);

	t_global_file_table* global_file_to_decrement = get_global_file_by_gfd(gfd);

	if (global_file_to_decrement == NULL)
		return NO_EXISTE_ARCHIVO;

	global_file_to_decrement->open--;

	int pos_gfd = get_pos_in_fs_global_table_by_gfd(gfd);

	if (global_file_to_decrement->open == 0) //Si es 0 lo borro de la Global File Table
		list_remove_and_destroy_element(fs_global_table, pos_gfd, &free);
	else
		list_replace(fs_global_table, pos_gfd, global_file_to_decrement);

	return NO_ERRORES;
}

int delete_file_from_global_table(int gfd) {
	log_debug(logger, "delete_file_from_global_table");
	int pos = get_pos_in_fs_global_table_by_gfd(gfd);
	t_global_file_table* g_file = get_global_file_by_gfd(gfd);

	if (pos < 0)
		return NO_EXISTE_ARCHIVO;
	else if (g_file->open > 1)
		return ARCHIVO_ABIERTO_POR_OTROS;

	//list_remove_and_destroy_element(fs_global_table, pos, &free); // lo borro de la global

	return NO_ERRORES;
}

bool is_allowed(int pid, int fd, char* flag) {
	log_debug(logger, "is_allowed");
	t_open_file* process = get_open_file_by_fd_and_pid(fd, pid);
	if (process == NULL || is_default(fd))
		return false;
	return string_contains(process->flag, flag);
}

void set_process_file_by_fd(int fd, int pid, t_open_file* n_file) {
	log_debug(logger, "set_process_file_by_fd");
	int pos_table = get_pos_in_fs_process_table_by_pid(pid);
	t_process_file_table* files_open_process = list_get(fs_process_table, pos_table);

	int pos_fd = get_pos_open_file_by_fd(files_open_process->open_files, fd);
	list_replace(files_open_process->open_files, pos_fd, n_file);
	list_replace(fs_process_table, pos_table, files_open_process);
}

bool set_pointer(int n_pointer, int fd, int pid) {
	log_debug(logger, "set_pointer");
	t_open_file* process = get_open_file_by_fd_and_pid(fd, pid);
	if (process == NULL || is_default(fd))
		return false;
	process->pointer = n_pointer;
	set_process_file_by_fd(fd, pid, process);
	return true;
}

bool is_default(int fd) {
	log_debug(logger, "is_default");
	if (fd == 0 || fd == 1 || fd == 2)
		return true;
	return false;
}

t_list* add_defaults_fds() {
	log_debug(logger, "add_defaults_fds");
	t_list* defaults = list_create();
	int i;
	for (i = 0; i < 3; i++) {
		t_open_file* file = malloc(sizeof(t_open_file));
		file->gfd = -1;
		file->fd = i;
		list_add(defaults, file);
	}
	return defaults;
}

int write_file(int fd_write, int pid, char* info, int size) {
	log_debug(logger, "write_file");
	t_socket_pcb* socket_console = find_socket_by_pid(pid);
	if (fd_write == 1) {
		runFunction(socket_console->socket, "kernel_print_message", 2, info, string_itoa(pid));
		log_debug(logger, "Escribir en Consola '%s'", info);
		return NO_ERRORES;
	}

	t_open_file* process = get_open_file_by_fd_and_pid(fd_write, pid);

	if (is_allowed(pid, fd_write, "w")) {
		char* path = get_path_by_gfd(process->gfd);
		int offset = process->pointer;
		runFunction(fs_socket, "kernel_save_data", 5, path, string_itoa(offset), string_itoa(size), info, string_itoa(pid));
		wait_response(&fs_mutex);
		if (!fs_response) {
			return ERROR_ESCRIBIR_ARCHIVO;
		}

		return NO_ERRORES;
	}

	return ESCRIBIR_SIN_PERMISOS;
}

void close_all_files_by_pid(int pid){
	int i;
	t_process_file_table* process = get_process_file_by_pid(pid);
	if (process == NULL) return;
	int size = list_size(process->open_files);
	for (i=3; i < size; i++){
		close_file(i, pid);
	}
}

void show_global_file_table() {
	log_debug(logger, "show_global_file_table");
	clear_screen();
	printf("FILES OPEN: %d\n", list_size(fs_global_table));
	if (list_size(fs_global_table) != 0) {
		int i;
		for (i = 0; i < list_size(fs_global_table); i++) {
			t_global_file_table* global_file = list_get(fs_global_table, i);
			printf("	> GLOBAL_FD: %d\t> OPEN: %d\t> FILE: %s\n", global_file->gfd, global_file->open, global_file->path);
		}
	}

	wait_any_key();
}

void show_all_processes_file_table() {
	log_debug(logger, "show_all_processes_file_table");
	int i;
	for (i = 0; i < list_size(fs_process_table); i++) {
		t_process_file_table* file = list_get(fs_process_table, i);
		printf("> PID %d:\n", file->pid);
		show_process_file_table(file->pid);
		puts("");
	}
}

void show_process_file_table(int pid) {
	log_debug(logger, "show_process_file_table");
	t_process_file_table* files_process = get_process_file_by_pid(pid);
	if (files_process == NULL)
		printf("FILES OPEN: 0\n");
	else {
		int i;
		printf("FILES OPEN: %d\n", list_size(files_process->open_files) -3);
		for (i = 3; i < list_size(files_process->open_files); i++) {
			t_open_file* file = list_get(files_process->open_files, i);
			printf("	> FD: %d\t> GLOBAL_FD: %d\t> FLAGS: %s\t> POINTER: %d\n", file->fd, file->gfd, file->flag, file->pointer);
		}
	}
}

/*
 * HEAP
 */
t_heap_stats* find_heap_stats_by_pid(int pid) {
	log_debug(logger, "find_heap_stats_by_pid");
	bool find(void* element) {
		t_heap_stats* n_heap = element;
		return n_heap->pid == pid;
	}
	t_heap_stats* heap = list_find(heap_stats_list, &find);
	return heap;
}
t_heap_manage* find_heap_manage_by_pid(int pid) {
	log_debug(logger, "find_heap_manage_by_pid");
	bool find(void* element) {
		t_heap_manage* n_heap = element;
		return n_heap->pid == pid;
	}
	t_heap_manage* heap = list_find(process_heap_pages, &find);
	return heap;
}
int find_heap_pages_pos_in_list(t_list* list, int pid) {
	log_debug(logger, "find_heap_pages_pos_in_list");
	int i;
	for (i = 0; i < list_size(list); i++) {
		t_heap_manage* f_heap = list_get(list, i);
		if (f_heap->pid == pid)
			return i;
	}
	return -1;
}

int get_suitable_page(int space, t_list* heap_pages, int start) {
	log_debug(logger, "get_suitable_page");
	int i;
	for (i = start; i < list_size(heap_pages); i++) {
		t_heap_page* heap = list_get(heap_pages, i);
		if (heap->free_size >= space && !heap->wasFreed)
			return heap->page_n;
	}
	return -1;
}

HeapMetadata* read_HeapMetadata(char* page, int offset) {
	log_debug(logger, "read_HeapMetadata");
	if (offset == mem_page_size)
		return NULL;

	HeapMetadata* heap_metadata = malloc(sizeof(HeapMetadata));
	int i, j = 0;
	char* aux = string_new();
	heap_metadata->isFree = page[offset] - '0'; //char to int
	for (i = offset + 1; (page[i] != '#' && j < 4); i++) {
		aux[j] = page[i];
		j++;
	}
	heap_metadata->size = atoi(aux);
	return heap_metadata;
}

int write_HeapMetadata(HeapMetadata* n_heap, int offset, char* page) {
	log_debug(logger, "write_HeapMetadata");
	int i, j = 0;

	page[offset] = n_heap->isFree ? '1' : '0';
	char* aux = string_new();
	aux = string_itoa(n_heap->size);

	int times = sizeof(uint32_t) - string_length(aux);

	string_append(&aux, string_repeat('-', times));

	for (i = offset + 1; j < string_length(aux); i++) {
		page[i] = aux[j];
		j++;
	}
	//free(n_heap);
	return offset + heap_metadata_size;
}
//Esta funcion ocupa un espacio en una pagina, caso contrario retorna -1
int it_fits_malloc(char* full_page, int space_occupied) {
	log_debug(logger, "it_fits_malloc");
	int offset;
	HeapMetadata* actual_heap;

	/*if (space_occupied > mem_page_size - (heap_metadata_size * 2))
	 return false; //Tamanio maximo alloc*/
	for (offset = 0; offset < mem_page_size;) {

		actual_heap = read_HeapMetadata(full_page, offset);

		//If magico, creanme que anda para todos los casos xd
		if (actual_heap->isFree && (actual_heap->size >= (space_occupied + heap_metadata_size) || ((actual_heap->size == space_occupied) && (offset + heap_metadata_size + space_occupied != mem_page_size)))) {
			actual_heap->isFree = false;
			if (space_occupied < actual_heap->size) {
				HeapMetadata* n_heap = malloc(sizeof(HeapMetadata));
				n_heap->isFree = true;
				n_heap->size = actual_heap->size - space_occupied - heap_metadata_size;
				//if(n_heap->size != 0) n_heap->size -= heap_metadata_size;
				write_HeapMetadata(n_heap, offset + heap_metadata_size + space_occupied, full_page);
				//free(n_heap);
			}
			actual_heap->size = space_occupied;
			return write_HeapMetadata(actual_heap, offset, full_page);
		} else {
			offset += actual_heap->size + heap_metadata_size;
		}
	}
	//free(actual_heap);
	return -1;
}
//Esta funcion libera una posicion de pagina y hace la parte de fragmentacion si hay dos bloques continuos libres y retorna la cantidad de espacio liberado o -1 si se libero toda la pagina
int free_heap(char* full_page, int pos) {
	log_debug(logger, "free_heap");
	HeapMetadata* heap = read_HeapMetadata(full_page, pos);
	heap->isFree = true;
	write_HeapMetadata(heap, pos, full_page);

	heap = read_HeapMetadata(full_page, pos);

	int freed_space = 0;
	freed_space += heap->size;

	int offset;
	HeapMetadata* next_heap;
	for (offset = 0; offset < mem_page_size;) {
		heap = read_HeapMetadata(full_page, offset);
		next_heap = read_HeapMetadata(full_page, heap->size + heap_metadata_size + offset);
		if (next_heap == NULL)
			break;
		if (heap->isFree && next_heap->isFree) {
			int i, start, end;
			start = offset + heap->size + heap_metadata_size;
			end = start + heap_metadata_size;

			heap->size += next_heap->size + heap_metadata_size;
			write_HeapMetadata(heap, offset, full_page);

			for (i = start; i < end; i++) {
				full_page[i] = '#';
			}
		}
		offset += heap->size + heap_metadata_size;
	}
	heap = read_HeapMetadata(full_page, 0);
	if (heap->isFree && (mem_page_size - heap->size - heap_metadata_size) == 0)
		return -1;
	else
		return freed_space;
}

int add_heap_page(int pid, t_heap_manage* heap_manage, int page, int space) {
	log_debug(logger, "add_heap_page");
	runFunction(mem_socket, "i_add_pages_to_program", 2, string_itoa(pid), string_itoa(page));
	wait_response(&mem_response);
	if (memory_response == NO_SE_PUEDEN_ASIGNAR_MAS_PAGINAS) {
		return NO_SE_PUEDEN_ASIGNAR_MAS_PAGINAS;
	}
	t_heap_page* n_heap_page = malloc(sizeof(t_heap_page));
	n_heap_page->free_size = mem_page_size - (heap_metadata_size * 2) - space;
	n_heap_page->page_n = page;
	n_heap_page->wasFreed = false;

	list_add(heap_manage->heap_pages, n_heap_page);
	int pos_to_replace = find_heap_pages_pos_in_list(process_heap_pages, pid);
	list_replace(process_heap_pages, pos_to_replace, heap_manage);
	runFunction(mem_socket, "i_read_bytes_from_page", 4, string_itoa(pid), string_itoa(page), string_itoa(0), string_itoa(mem_page_size));
	wait_response(&mem_response);

	HeapMetadata* heap = malloc(sizeof(HeapMetadata));
	heap->isFree = true;
	heap->size = mem_page_size - heap_metadata_size;
	mem_read_buffer = string_repeat('#', mem_page_size);
	write_HeapMetadata(heap, 0, mem_read_buffer);
	//free(heap);
	it_fits_malloc(mem_read_buffer, space);
	runFunction(mem_socket, "i_store_bytes_in_page", 5, string_itoa(pid), string_itoa(page), string_itoa(0), string_itoa(mem_page_size), mem_read_buffer);
	wait_response(&mem_response);
	log_debug(logger, "add_heap_page: mem_offset_abs '%d'", mem_offset_abs);
	return (mem_offset_abs + heap_metadata_size);
}

void occupy_space(int pid, int page, int space, bool freed) {
	log_debug(logger, "occupy_space");
	t_heap_manage* heap_manage = find_heap_manage_by_pid(pid);
	int i;
	for (i = 0; i < list_size(heap_manage->heap_pages); i++) {
		t_heap_page* heap_page = list_get(heap_manage->heap_pages, i);
		if (heap_page->page_n == page) {
			heap_page->free_size -= space;
			heap_page->wasFreed = freed;
		}
	}
}

int malloc_memory(int pid, int size) {
	log_debug(logger, "malloc_memory");
	if (size > mem_page_size - (heap_metadata_size * 2))
		return RESERVAR_MAS_MEMORIA_TAMANIO_PAGINA; //directamente lo rechazo porque excede el tamanio maximo del alloc

	pcb* pcb_malloc = find_pcb_by_pid(pid);
	t_heap_manage* heap_manage = find_heap_manage_by_pid(pid);
	t_heap_stats* heap_stats = find_heap_stats_by_pid(pid);

	heap_stats->malloc_c++;
	heap_stats->malloc_b += size;

	int page = pcb_malloc->page_c + stack_size;

	if (list_is_empty(heap_manage->heap_pages)) {
		return add_heap_page(pid, heap_manage, page, size);
	} else {
		int i;
		for (i = 0; i < list_size(heap_manage->heap_pages); i++) {
			page = get_suitable_page(size, heap_manage->heap_pages, i);
			if (page != -1) {
				runFunction(mem_socket, "i_read_bytes_from_page", 4, string_itoa(pid), string_itoa(page), string_itoa(0), string_itoa(mem_page_size));
				wait_response(&mem_response);
				int it_fits = it_fits_malloc(mem_read_buffer, size);
				if (it_fits != -1) {
					runFunction(mem_socket, "i_store_bytes_in_page", 5, string_itoa(pid), string_itoa(page), string_itoa(0), string_itoa(mem_page_size), mem_read_buffer);
					wait_response(&mem_response);
					occupy_space(pid, page, size + heap_metadata_size, false);
					return (mem_offset_abs + it_fits);
				}
			}
		}
		t_heap_page* last_page = list_get(heap_manage->heap_pages, list_size(heap_manage->heap_pages) - 1);
		return add_heap_page(pid, heap_manage, (last_page->page_n) + 1, size);
	}
}

void free_memory(int pid, int pointer) {
	log_debug(logger, "free_memory");
	runFunction(mem_socket, "i_get_page_from_pointer", 1, string_itoa(pointer));
	wait_response(&mem_response);
	runFunction(mem_socket, "i_read_bytes_from_page", 4, string_itoa(pid), string_itoa(page_from_pointer), string_itoa(0), string_itoa(mem_page_size));
	wait_response(&mem_response);

	int frame_c = pointer / mem_page_size;
	int offset_rel = pointer - mem_page_size * frame_c;
	int freed_space = free_heap(mem_read_buffer, offset_rel - heap_metadata_size);

	t_heap_stats* heap_stats = find_heap_stats_by_pid(pid);
	heap_stats->free_c++;
	if (freed_space < 0)
		heap_stats->free_b += mem_page_size - (2 * heap_metadata_size);
	else
		heap_stats->free_b += freed_space;

	if (freed_space == -1) {
		runFunction(mem_socket, "i_free_page", 2, string_itoa(pid), string_itoa(page_from_pointer));
		wait_response(&mem_response);
		occupy_space(pid, page_from_pointer, -freed_space, true);
	} else {
		occupy_space(pid, page_from_pointer, -freed_space, false);
		runFunction(mem_socket, "i_store_bytes_in_page", 5, string_itoa(pid), string_itoa(page_from_pointer), string_itoa(0), string_itoa(mem_page_size), mem_read_buffer);
		wait_response(&mem_response);
	}
}

void wait_response(pthread_mutex_t* mutex) {
	log_debug(logger, "wait_response");
	pthread_mutex_lock(mutex);
}

void signal_response(pthread_mutex_t* mutex) {
	log_debug(logger, "signal_response");
	pthread_mutex_unlock(mutex);
}

bool remove_program_code_by_pid(int pid){
	log_debug(logger, "remove_program_code_by_pid: start");
	void free_program(void* element) {
		t_program* program = element;
			free(program);
	}
	bool result = false;
	int i;
	for(i = 0; i < list_size(program_list); i++) {
		t_program* program = list_get(program_list, i);
		if(program->pid == pid) {
			list_remove_and_destroy_element(program_list, i, &free_program);
			result = true;
			break;
		}
	}
	log_debug(logger, "remove_program_code_by_pid: end");

	return result;
}

void create_function_dictionary() {
	log_debug(logger, "create_function_dictionary");
	fns = dictionary_create();

	//CONSOLE
	dictionary_put(fns, "console_load_program", &console_load_program);
	dictionary_put(fns, "console_abort_program", &console_abort_program);
	//MEMORY
	dictionary_put(fns, "memory_identify", &memory_identify);
	dictionary_put(fns, "memory_response_start_program", &memory_response_start_program);
	dictionary_put(fns, "memory_page_size", &memory_page_size);
	dictionary_put(fns, "memory_response_heap", &memory_response_heap);
	dictionary_put(fns, "memory_response_read_bytes_from_page", &memory_response_read_bytes_from_page);
	dictionary_put(fns, "memory_response_store_bytes_in_page", &memory_response_store_bytes_in_page);
	dictionary_put(fns, "memory_response_get_page_from_pointer", &memory_response_get_page_from_pointer);
	//CPU
	dictionary_put(fns, "cpu_has_quantum_changed", &cpu_has_quantum_changed);
	dictionary_put(fns, "cpu_has_aborted", &cpu_has_aborted);
	dictionary_put(fns, "cpu_received_page_stack_size", &cpu_received_page_stack_size);
	dictionary_put(fns, "cpu_task_finished", &cpu_task_finished);
	dictionary_put(fns, "cpu_get_shared_var", &cpu_get_shared_var);
	dictionary_put(fns, "cpu_set_shared_var", &cpu_set_shared_var);
	dictionary_put(fns, "cpu_wait_sem", &cpu_wait_sem);
	dictionary_put(fns, "cpu_signal_sem", &cpu_signal_sem);
	dictionary_put(fns, "cpu_malloc", &cpu_malloc);
	dictionary_put(fns, "cpu_free", &cpu_free);
	dictionary_put(fns, "cpu_open_file", &cpu_open_file);
	dictionary_put(fns, "cpu_close_file", &cpu_close_file);
	dictionary_put(fns, "cpu_delete_file", &cpu_delete_file);
	dictionary_put(fns, "cpu_write_file", &cpu_write_file);
	dictionary_put(fns, "cpu_read_file", &cpu_read_file);
	dictionary_put(fns, "cpu_seek_file", &cpu_seek_file);
	dictionary_put(fns, "cpu_validate_file", &cpu_validate_file);
	//FILESYSTEM
	dictionary_put(fns, "fs_response_file", &fs_response_file);
	dictionary_put(fns, "fs_response_get_data", &fs_response_get_data);

}

void open_socket(t_config* config, char* name) {
	log_debug(logger, "open_socket");
	int port;

	if (!strcmp(name, CONSOLE))
		port = config_get_int_value(config, PUERTO_PROG);
	else if (!strcmp(name, CPU))
		port = config_get_int_value(config, PUERTO_CPU);
	else {
		log_error(logger, "Error at creating listener for %s", name);
		error_show(" at creating listener for %s", name);
		exit(EXIT_FAILURE);
	}

	if (createListen(port, &newClient, fns, &connectionClosed, NULL) == -1) {
		log_error(logger, "Error at creating listener for %s", name);
		error_show(" at creating listener for %s", name);
		exit(EXIT_FAILURE);
	}
	log_debug(logger, "Listening new clients of %s at %d.\n", name, port);
}

void connect_to_server(t_config* config, char* name) {
	log_debug(logger, "connect_to_server");
	int port;
	char* ip_server;
	int* socket;

	if (!strcmp(name, FILESYSTEM)) {
		ip_server = config_get_string_value(config, IP_FS);
		port = config_get_int_value(config, PUERTO_FS);
		socket = &fs_socket;
	} else if (!strcmp(name, MEMORY)) {
		ip_server = config_get_string_value(config, IP_MEMORIA);
		port = config_get_int_value(config, PUERTO_MEMORIA);
		socket = &mem_socket;
	} else {
		log_error(logger, "Error at connecting to %s.", name);
		error_show(" at connecting to %s.\n", name);
		exit(EXIT_FAILURE);
	}

	if ((*socket = connectServer(ip_server, port, fns, &server_connectionClosed,
	NULL)) == -1) {
		log_error(logger, "Error at connecting to %s.", name);
		error_show(" at connecting to %s.\n", name);
		exit(EXIT_FAILURE);
	}
	log_debug(logger, "Connected to %s.", name);
}

void init_kernel(t_config* config) {
	log_debug(logger, "init_kernel");

	pthread_mutex_init(&abort_console_mutex, NULL);
	pthread_mutex_init(&cpu_mutex, NULL);
	pthread_mutex_init(&p_counter_mutex, NULL);
	pthread_mutex_init(&console_mutex, NULL);
	pthread_mutex_init(&pcb_list_mutex, NULL);
	pthread_mutex_init(&planning_mutex, NULL);
	pthread_mutex_init(&process_in_memory_mutex, NULL);
	pthread_mutex_init(&shared_vars_mutex, NULL);
	pthread_mutex_init(&sems_mutex, NULL);
	pthread_mutex_init(&sem_pid_mutex, NULL);
	pthread_mutex_init(&sems_blocked_list, NULL);
	pthread_mutex_init(&program_list_mutex, NULL);
	pthread_mutex_init(&json_mutex, NULL);

	pthread_mutex_init(&mem_response, NULL);
	pthread_mutex_init(&fs_mutex, NULL);
	pthread_mutex_init(&fs_request_mutex, NULL);

	pthread_mutex_lock(&mem_response);
	pthread_mutex_lock(&fs_mutex);

	can_check_programs = false;

	port_con = config_get_int_value(config, PUERTO_PROG);
	port_cpu = config_get_int_value(config, PUERTO_CPU);
	multiprog = config_get_int_value(config, GRADO_MULTIPROG);
	stack_size = config_get_int_value(config, STACK_SIZE);
	quantum_sleep = config_get_int_value(config, QUANTUM_SLEEP);

	shared_vars = list_create();
	char** global_vars_arr = config_get_array_value(config, SHARED_VARS);
	int i = 0;
	while (global_vars_arr[i] != NULL) {
		t_shared_var* shared_var = malloc(sizeof(t_shared_var));
		shared_var->var = string_substring_from(global_vars_arr[i], 1);
		shared_var->value = 0;
		list_add(shared_vars, shared_var);
		i++;
	}

	sem_ids = list_create();
	char** sem_ids_arr = config_get_array_value(config, SEM_IDS);
	char** sem_vals_arr = config_get_array_value(config, SEM_INIT);
	i = 0;
	while (sem_ids_arr[i] != NULL) {
		t_sem* sem = malloc(sizeof(t_sem));
		sem->id = sem_ids_arr[i];
		sem->value = atoi(sem_vals_arr[i]);
		sem->init_value = atoi(sem_vals_arr[i]);
		sem->blocked_pids = "[]";
		list_add(sem_ids, sem);
		i++;
	}

	char* p = config_get_string_value(config, ALGORITMO);
	if (!strcmp(p, "FIFO")) {
		planning_alg = FIFO;
		quantum = 0;
	} else if (!strcmp(p, "RR")) {
		planning_alg = RR;
		quantum = config_get_int_value(config, QUANTUM) - 1;
	}

	program_list = list_create();

	p_counter = 0;
	process_in_memory = 0;
	planning_running = true;

	new_queue = queue_create();
	ready_list = list_create();
	exec_list = list_create();
	block_list = list_create();
	exit_queue = queue_create();

	socket_pcb_list = list_create();

	sem_pid_list = list_create();

	cpu_list = list_create();

	fs_process_table = list_create();
	fs_global_table = list_create();
	pending_process_to_kill = list_create();

	heap_stats_list = list_create();
	process_heap_pages = list_create();
	heap_metadata_size = sizeof(bool) + sizeof(uint32_t);

	open_socket(config, CONSOLE);
	open_socket(config, CPU);
	connect_to_server(config, FILESYSTEM);
	connect_to_server(config, MEMORY);

	mem_read_buffer = malloc(sizeof(mem_page_size));
}

void print_menu() {
	log_debug(logger, "print_menu");
	clear_screen();

	show_title("KERNEL - MAIN MENU");
	println("Enter your choice:");
	println("> [A]CTIVE PROCESS");
	println("> SHARED [V]ARS & SEMS");
	println("> [I]NFO");
	println("> [F]ILE_TABLE");
	println("> [M]ULTIPROGRAMMING");
	println("> [S]TOP PROCESS");
	if (planning_running)
		println("> STOP [P]LANIFICATION\n");
	else
		println("> CONTINUE [P]LANIFICATION\n");
}

void ask_option(char *sel) {
	log_debug(logger, "ask_option");
	print_menu();
	fgets(sel, 255, stdin);
	strtok(sel, "\n");
	string_to_upper(sel);
}

void show_active_process(char option) {
	log_debug(logger, "show_active_process");
	pthread_mutex_lock(&pcb_list_mutex);
	t_list* list = list_create();
	char* list_name = string_new();
	int i, j;
	switch (option) {
		case 'A':
			string_append(&list_name, "PROCESS IN ALL\n");

			j = queue_size(new_queue);
			for (i = 0; i < j; i++) {
				pcb* n_pcb = queue_pop(new_queue);
				queue_push(new_queue, n_pcb);
				list_add(list, n_pcb);
			}

			list_add_all(list, ready_list);
			list_add_all(list, block_list);
			list_add_all(list, exec_list);

			j = queue_size(exit_queue);
			for (i = 0; i < j; i++) {
				pcb* n_pcb = queue_pop(exit_queue);
				queue_push(exit_queue, n_pcb);
				list_add(list, n_pcb);
			}
			break;
		case 'N':
			string_append(&list_name, "PROCESS IN NEW\n");
			int i, j = queue_size(new_queue);
			for (i = 0; i < j; i++) {
				pcb* n_pcb = queue_pop(new_queue);
				queue_push(new_queue, n_pcb);
				list_add(list, n_pcb);
			}
			break;
		case 'R':
			string_append(&list_name, "PROCESS IN READY\n");
			list_add_all(list, ready_list);
			break;
		case 'E':
			string_append(&list_name, "PROCESS IN EXIT\n");
			j = queue_size(exit_queue);
			for (i = 0; i < j; i++) {
				pcb* n_pcb = queue_pop(exit_queue);
				queue_push(exit_queue, n_pcb);
				list_add(list, n_pcb);
			}
			break;
		case 'X':
			string_append(&list_name, "PROCESS IN EXEC\n");
			list_add_all(list, exec_list);
			break;
		case 'B':
			string_append(&list_name, "PROCESS IN BLOCK\n");
			list_add_all(list, block_list);
			break;
	}

	clear_screen();
	printf("%s", list_name);
	char* pcb_state = string_new();
	for (i = 0; i < list_size(list); i++) {
		pcb* n_pcb = list_get(list, i);
		printf("%s%d%s ", n_pcb->pid < 10 ? " " : "", n_pcb->pid, n_pcb->pid < 10 ? "" : " ");

		if(option == 'A') {
			switch(n_pcb->state) {
				case 1:
					string_append(&pcb_state, " N ");
					break;
				case 2:
					string_append(&pcb_state, " R ");
					break;
				case 3:
					string_append(&pcb_state, " X ");
					break;
				case 4:
					string_append(&pcb_state, " B ");
					break;
				case 5:
					string_append(&pcb_state, " E ");
					break;
			}
		}
	}
	if(option == 'A')
		printf("\n%s\n", pcb_state);

	pthread_mutex_unlock(&pcb_list_mutex);
	wait_any_key();
}

void do_show_active_process(char* sel) {
	log_debug(logger, "do_show_active_process");
	if (!strcmp(sel, "A")) {
		char queue[255];
		println("> [A] ALL");
		println("> [N] NEW");
		println("> [R] READY");
		println("> [E] EXIT");
		println("> [X] EXEC");
		println("> [B] BLOCK");
		printf("> QUEUE: ");
		fgets(queue, sizeof(queue), stdin);
		strtok(queue, "\n");
		string_to_upper(queue);

		show_active_process(queue[0]);
	}
}

// Cantidad de páginas de Heap
void show_heap_pages(int pid) {
	log_debug(logger, "show_heap_pages");
	t_heap_manage* heap_manage = find_heap_manage_by_pid(pid);
	int pages = 0;
	if (heap_manage != NULL)
		pages = list_size(heap_manage->heap_pages);

	printf("HEAP PAGES: %d\n", pages);
}

// Cantidad de acciones alocar en operaciones y bytes
void show_allocates(int pid) {
	log_debug(logger, "show_allocates");
	t_heap_stats* heap_stats = find_heap_stats_by_pid(pid);

	printf("HEAP MALLOC NUM: %d\n", heap_stats->malloc_c);
	printf("HEAP MALLOC BYTES: %d\n", heap_stats->malloc_b);
}

// Cantidad de acciones liberar en operaciones y bytes
void show_free(int pid) {
	log_debug(logger, "show_free");
	t_heap_stats* heap_stats = find_heap_stats_by_pid(pid);

	printf("HEAP FREE NUM: %d\n", heap_stats->free_c);
	printf("HEAP FREE BYTES: %d\n", heap_stats->free_b);
}

// Cantidad de syscalls
void show_syscalls(int pid) {
	log_debug(logger, "show_syscalls");
	pcb* n_pcb = find_pcb_by_pid(pid);

	printf("SYSCALLS NUM: %d\n", n_pcb->statistics.op_priviliges);
}

void show_info(int pid) {
	log_debug(logger, "show_info");
	pcb* n_pcb = find_pcb_by_pid2(pid);
	if (n_pcb == NULL) return;
	char* info = string_new();
	string_append_with_format(&info, "PID: %d\n", n_pcb->pid);
	string_append_with_format(&info, "\nRAFAGAS EJECUTADAS: %d", n_pcb->statistics.cycles);

	clear_screen();
	printf("%s\n", info);

	show_syscalls(pid);
	show_process_file_table(pid);
	show_heap_pages(pid);
	show_allocates(pid);
	show_free(pid);

	wait_any_key();
}

void shared_vars_and_sems() {
	int i = 0;
	clear_screen();
	printf("SHARED VARS:\n");

	pthread_mutex_lock(&shared_vars_mutex);

	for(i = 0; i < list_size(shared_vars); i++) {
		t_shared_var* shared_var = list_get(shared_vars, i);
		printf("\t%s = %d\n", shared_var->var, shared_var->value);
	}

	pthread_mutex_unlock(&shared_vars_mutex);

	printf("\nSEMS:\n");

	for(i = 0; i < list_size(sem_ids); i++) {
		t_sem* sem = list_get(sem_ids, i);
		printf("\t%s = %d --> %s\n", sem->id, sem->value, sem->blocked_pids);
	}

	wait_any_key();
}

void do_shared_vars_and_sems(char* sel) {
	if (!strcmp(sel, "V")) {
		shared_vars_and_sems();
	}
}

void do_show_info(char* sel) {
	if (!strcmp(sel, "I")) {
		char pid[255];
		printf("> PID: ");
		fgets(pid, sizeof(pid), stdin);
		strtok(pid, "\n");

		show_info(atoi(pid));
	}
}

void do_show_file_table(char* sel) {
	if (!strcmp(sel, "F")) {
		show_global_file_table();
	}
}

void check_new_list() {
	void free_program(void* element) {
			t_program* program = element;
			free(program);
	}
	if(queue_size(new_queue) > 0) {
		pcb* new_pcb = queue_peek(new_queue);
		int i;
		for(i = 0; i < list_size(program_list); i++) {
			t_program* program = list_get(program_list, i);
			if(program->pid == new_pcb->pid) {
				process_struct.socket = program->socket;
				process_struct.pid = new_pcb->pid;
				process_struct.state = new_pcb->state;
				process_struct.list_pos = program->list_pos;

				runFunction(mem_socket, "i_start_program", 2, string_itoa(new_pcb->pid), program->program);
				list_remove_and_destroy_element(program_list, i, &free_program);
				break;
			}
		}
	} else
		can_check_programs = true;
}

void kill_pending_process(){
	int i;
	for (i=0; i < list_size(pending_process_to_kill); i++){
		int pid = list_get(pending_process_to_kill, i);
		log_debug(logger, "kill_pending_process: pid: %d", pid);
		stop_process(pid);
		list_remove(pending_process_to_kill, i);
		pcb* n_pcb = find_pcb_by_pid(pid);
		n_pcb->exit_code = FINALIZADO_CONSOLA;
	}
}

void do_change_multiprogramming(char* sel) {
	if (!strcmp(sel, "M")) {
		char multi[255];
		printf("> Num Multiprog [%d]: ", multiprog);
		fgets(multi, sizeof(multi), stdin);
		strtok(multi, "\n");

		multiprog = atoi(multi);

		if (process_in_memory + 1 <= multiprog)
			check_new_list();
	}
}

t_socket_pcb* find_socket_by_pid(int pid) {
	log_debug(logger, "find_socket_by_pid");
	pthread_mutex_lock(&pcb_list_mutex);
	bool find(void* element) {
		t_socket_pcb* n_socket_pcb = element;
		return n_socket_pcb->pid == pid;
	}

	t_socket_pcb* n_socket_pcb = list_find(socket_pcb_list, &find);
	pthread_mutex_unlock(&pcb_list_mutex);
	return n_socket_pcb;
}

void remove_from_list_sems(int pid) {
	int i, j;
	for(i = 0; i < list_size(sem_ids); i++) {
		pthread_mutex_lock(&sems_blocked_list);
		t_sem* sem = list_get(sem_ids, i);
		char** list_blocked_pids = string_get_string_as_array(sem->blocked_pids);
		char* temp = string_new();

		j = 0;
		while(list_blocked_pids[j] != NULL) {
			if(atoi(list_blocked_pids[j]) != pid) {
				pcb* p = find_pcb_by_pid(atoi(list_blocked_pids[j]));
				if(p->state != EXIT_LIST)
					string_append_with_format(&temp, "%s,", list_blocked_pids[j]);
				else if(sem->value < sem->init_value)
					sem->value++;
			} else if(sem->value < sem->init_value)
				sem->value++;

			free(list_blocked_pids[j]);
			j++;
		}

		if (string_length(temp) > 0)
			temp = string_substring_until(temp, string_length(temp) - 1);

		sem->blocked_pids = string_new();
		string_append_with_format(&sem->blocked_pids, "[%s]", temp);

		free(list_blocked_pids);
		pthread_mutex_unlock(&sems_blocked_list);

		pthread_mutex_lock(&sems_mutex);
		int process = get_first(sem->blocked_pids);
		if(process > -1) {
			pcb* _pcb = find_pcb_by_pid(process);
			if(_pcb->state == BLOCK_LIST && sem->value >= 0) {
				char* temp = remove_first(sem->blocked_pids);
				sem->blocked_pids = string_new();
				string_append(&sem->blocked_pids, temp);
				sem->value++;
				move_to_list(_pcb, READY_LIST);
				short_planning();
			}
		}
		pthread_mutex_unlock(&sems_mutex);
	}
}

void stop_process(int pid) {
	//log_debug(logger, "stop_process");
	pcb* l_pcb = find_pcb_by_pid2(pid);
	if(l_pcb == NULL){
		log_debug(logger, "stop_process doesnt exist pid %d", pid);
		return;
	}

	void free_heap(void* element) {
		t_heap_manage* heap = element;
		free(heap);
	}

	l_pcb->exit_code = FINALIZADO_KERNEL;
	log_debug(logger, "stop_process: pid: '%d', state: '%d", pid, l_pcb->state);
	if (l_pcb->state != EXEC_LIST) {
		if(l_pcb->state == BLOCK_LIST)
			remove_from_list_sems(l_pcb->pid);
		int heap_pos = find_heap_pages_pos_in_list(process_heap_pages, l_pcb->pid);
		if(heap_pos >= 0)
			list_remove_and_destroy_element(process_heap_pages, heap_pos, &free_heap);
		close_all_files_by_pid(l_pcb->pid);
		substract_process_in_memory();
		runFunction(mem_socket, "i_finish_program", 1, string_itoa(l_pcb->pid));
		move_to_list(l_pcb, EXIT_LIST);

		if (l_pcb->state != BLOCK_LIST && process_in_memory + 1 <= multiprog)
			check_new_list();
	}

	t_socket_pcb* socket_pcb = find_socket_by_pid(pid);
	runFunction(socket_pcb->socket, "kernel_stop_process", 2, string_itoa(pid), string_itoa(FINALIZADO_KERNEL));
}

void do_stop_process(char* sel) {
	if (!strcmp(sel, "S")) {
		char pid[255];
		printf("> PID: ");
		fgets(pid, sizeof(pid), stdin);
		strtok(pid, "\n");

		stop_process(atoi(pid));
	}
}

void do_stop_planification(char* sel) {
	if (!strcmp(sel, "P")) {
		planning_running = !planning_running;
		if (planning_running)
			short_planning();
	}
}

void init_notify(char* path_file) {
	if (watch_descriptor > 0 && fd_inotify > 0) {
		inotify_rm_watch(fd_inotify, watch_descriptor);
	}
	fd_inotify = inotify_init();
	if (fd_inotify > 0) {
		watch_descriptor = inotify_add_watch(fd_inotify, path_file, IN_MODIFY);
	}
}

void do_notify(int argc) {
	char buffer[BUF_LEN];
	int length = read(fd_inotify, buffer, BUF_LEN);
	int offset = 0;
	while (offset < length) {

		struct inotify_event *event = (struct inotify_event *) &buffer[offset];

		if (event->len) {
			if (event->mask & IN_CREATE) {
				if (event->mask & IN_ISDIR) {
					printf("The directory %s was created.\n", event->name);
				} else {
					printf("The file %s was created.\n", event->name);
				}
			} else if (event->mask & IN_DELETE) {
				if (event->mask & IN_ISDIR) {
					printf("The directory %s was deleted.\n", event->name);
				} else {
					printf("The file %s was deleted.\n", event->name);
				}
			} else if (event->mask & IN_MODIFY) {
				if (event->mask & IN_ISDIR) {
					printf("The directory %s was modified.\n", event->name);
				} else {
					printf("The file %s was modified.\n", event->name);
				}
			}
		}
		offset += sizeof(struct inotify_event) + event->len;
	}
}

void thread_continuous_scan_notify(int argc) {
	while (1) {
		FD_ZERO(&readfds);
		FD_SET(fd_inotify, &readfds);
		if (max_fd < fd_inotify)
			max_fd = fd_inotify;
		waiting.tv_sec = 0;
		waiting.tv_usec = 10;
		select(max_fd + 1, &readfds, NULL, NULL, &waiting);
		//log_debug(logger, "thread_continuous_scan_notify");
		if (FD_ISSET(fd_inotify, &readfds)) {
			log_debug(logger, "file config modified: QUANTUM_SLEEP: '%d'", quantum_sleep);
			do_notify(argc);
			config_destroy(config);
			config = malloc(sizeof(t_config));
			load_config(&config, argc, path_config);
			print_config(config, CONFIG_FIELDS, CONFIG_FIELDS_N);
			quantum_sleep = config_get_int_value(config, "QUANTUM_SLEEP");
		}
	}
}
int main(int argc, char *argv[]) {
	clear_screen();
	char sel[255];
	config = malloc(sizeof(t_config));

	remove("log");
	logger = log_create("log", "KERNEL", false, LOG_LEVEL_DEBUG);
	create_function_dictionary();

	path_config = string_from_format("%s", argv[1]);
	load_config(&config, argc, path_config);
	print_config(config, CONFIG_FIELDS, CONFIG_FIELDS_N);

	init_kernel(config);
	quantum_sleep = config_get_int_value(config, QUANTUM_SLEEP);

	init_notify(path_config);
	pthread_t notify_thread;
	pthread_create(&notify_thread, NULL, thread_continuous_scan_notify, argc);

	wait_any_key();

	ask_option(sel);
	do {
		do_show_active_process(sel);
		do_shared_vars_and_sems(sel);
		do_show_info(sel);
		do_show_file_table(sel);
		do_change_multiprogramming(sel);
		do_stop_process(sel);
		do_stop_planification(sel);
		ask_option(sel);
	} while (true);

	return EXIT_SUCCESS;
}
