#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <iostream>
#include <conio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
using namespace std;
//常量
/*
*	一个文件可以用4个直接地址，1个一级间址，1个二级间址（未用），用在inode结构中的di_addr[]
*/
const unsigned int NADDR = 6;
/*
*	磁盘块的最大容量512B
*/
const unsigned short BLOCK_SIZE = 512;
/*
*	一个文件的最大容量（没考虑二级间址）
*/
const unsigned int FILE_SIZE_MAX = (NADDR - 2) * BLOCK_SIZE + BLOCK_SIZE / sizeof(int) * BLOCK_SIZE;
/*
*	数据磁盘块的最大数量512个
*/
const unsigned short BLOCK_NUM = 512;
/*
*	inode结点的容量128B
*/
const unsigned short INODE_SIZE = 128;
/*
*	inode结点的最大数量256个
*/
const unsigned short INODE_NUM = 256;
/*
*	索引节点链的开始位置，前四个块用于加载程序扇区(本程序中为空)、超级块、索引节点位图和块位图。
*/
const unsigned int INODE_START = 3 * BLOCK_SIZE;
/*
*	数据块的起始位置
*/
const unsigned int DATA_START = INODE_START + INODE_NUM * INODE_SIZE;
/*
*	文件系统用户的最大数量10
*/
const unsigned int ACCOUNT_NUM = 10;
/*
*	一个目录中子文件和子目录的最大数量16
*/
const unsigned int DIRECTORY_NUM = 16;
/*
*	文件名称的最大长度14
*/
const unsigned short FILE_NAME_LENGTH = 14;
/*
*	用户名称最大长度14
*/
const unsigned short USER_NAME_LENGTH = 14;
/*
*	用户密码最大长度14
*/
const unsigned short USER_PASSWORD_LENGTH = 14;
/*
*	文件的最大权限111111111-511
*/
const unsigned short MAX_PERMISSION = 511;
/*
*	owner文件最大权限111000000-448
*/
const unsigned short MAX_OWNER_PERMISSION = 448;
//权限
const unsigned short ELSE_X = 1;
const unsigned short ELSE_W = 1 << 1;
const unsigned short ELSE_R = 1 << 2;
const unsigned short GRP_X = 1 << 3;
const unsigned short GRP_W = 1 << 4;
const unsigned short GRP_R = 1 << 5;
const unsigned short OWN_X = 1 << 6;
const unsigned short OWN_W = 1 << 7;
const unsigned short OWN_R = 1 << 8;
//数据结构
/*
* inode(128B)
*/
struct inode
{
	unsigned int i_ino;			//Identification of the inode.
	unsigned int di_addr[NADDR];//Number of data blocks where the file stored.
	unsigned short di_number;	//Number of associated files.
	unsigned short di_mode;		//0 stand for directory 1 stand for file
	unsigned short icount;		//link number
	unsigned short permission;	//file permission
	unsigned short di_uid;		//File's user id.
	unsigned short di_grp;		//File's group id
	unsigned short di_size;		//File size.
	char time[83];
};
/*
*	超级块
*/
struct filsys
{
	unsigned short s_num_inode;			//Total number of inodes.
	unsigned short s_num_finode;		//Total number of free inodes.
	unsigned short s_size_inode;		//Size of an inode.

	unsigned short s_num_block;			//Total number of blocks.
	unsigned short s_num_fblock;		//Total number of free blocks.
	unsigned short s_size_block;		//Size of a block.

	unsigned int special_stack[50];
	int special_free;
};
/*
*	目录文件
*/
struct directory
{
	char fileName[20][FILE_NAME_LENGTH];
	unsigned int inodeID[DIRECTORY_NUM];
};
/*
*	用户文件
*/
struct userPsw
{
	unsigned short userID[ACCOUNT_NUM];
	char userName[ACCOUNT_NUM][USER_NAME_LENGTH];
	char password[ACCOUNT_NUM][USER_PASSWORD_LENGTH];
	unsigned short groupID[ACCOUNT_NUM];
};
/*
*	函数声明
*/
void CommParser(inode*&);
void Help();
void Sys_start();
//全局变量
/*
*	模拟文件系统的文件的描述符
*/
FILE* fd = NULL;
/*
*	超级块
*/
filsys superBlock;
/*
*	信息节点位图 元素1代表“used”，0代表“free”
*/
unsigned short inode_bitmap[INODE_NUM];
/*
*	用户信息
*/
userPsw users;
/*
*	当前用户ID
*/
unsigned short userID = ACCOUNT_NUM;
/*
*	当前用户名 在命令行中使用
*/
char userName[USER_NAME_LENGTH + 6];
/*
*	当前目录
*/
directory currentDirectory;
/*
*	当前目录数组
*/
char ab_dir[100][14];
unsigned short dir_pointer;

//find free block
void find_free_block(unsigned int &inode_number)
{
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock, sizeof(filsys), 1, fd);
	if (superBlock.special_free == 0)
	{
		if (superBlock.special_stack[0] == 0)
		{
			printf("No value block!\n");
			return;
		}
		unsigned int stack[51];
		for (int i = 0; i < 50; i++)
		{
			stack[i] = superBlock.special_stack[i];
		}
		stack[50] = superBlock.special_free;
		fseek(fd, DATA_START + (superBlock.special_stack[0] - 50) * BLOCK_SIZE, SEEK_SET);
		fwrite(stack, sizeof(stack), 1, fd);
		fseek(fd, DATA_START + superBlock.special_stack[0] * BLOCK_SIZE, SEEK_SET);
		fread(stack, sizeof(stack), 1, fd);
		for (int i = 0; i < 50; i++)
		{
			superBlock.special_stack[i] = stack[i];
		}
		superBlock.special_free = stack[50];
	}
	inode_number = superBlock.special_stack[superBlock.special_free];
	superBlock.special_free--;
	superBlock.s_num_fblock--;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
}

//recycle block
void recycle_block(unsigned int &inode_number)
{
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock, sizeof(filsys), 1, fd);
	if (superBlock.special_free == 49)
	{
		unsigned int block_num;
		unsigned int stack[51];
		if (superBlock.special_stack[0] == 0)
			block_num = 499;
		else
			block_num = superBlock.special_stack[0] - 50;
		for (int i = 0; i < 50; i++)
		{
			stack[i] = superBlock.special_stack[i];
		}
		stack[50] = superBlock.special_free;
		fseek(fd, DATA_START + block_num*BLOCK_SIZE, SEEK_SET);
		fwrite(stack, sizeof(stack), 1, fd);
		block_num -= 50;
		fseek(fd, DATA_START + block_num*BLOCK_SIZE, SEEK_SET);
		fread(stack, sizeof(stack), 1, fd);
		for (int i = 0; i < 50; i++)
		{
			superBlock.special_stack[i] = stack[i];
		}
		superBlock.special_free = stack[50];
	}
	superBlock.special_free++;
	superBlock.s_num_fblock++;
	superBlock.special_stack[superBlock.special_free] = inode_number;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
}
/*
*	Formatting function of the file system, including the establishment
*	of superblock, inode chain, root directory, password file and so on.
*
*	return: the function return true only the file system is initialized
*			successfully.
*/
bool Format()
{
	/*
	*	创建个han文件
	*/
	FILE* fd = fopen("./fs.han", "wb+");
	if (fd == NULL)
	{
		printf("Fail to initialize the file system!\n");
		return false;
	}
	/*
	*	初始化超级块
	*/
	filsys superBlock;
	superBlock.s_num_inode = INODE_NUM;
	superBlock.s_num_block = BLOCK_NUM + 3 + 64; //3代表0空闲块、1超级块、2Inode位示图表,64块存inode
	superBlock.s_size_inode = INODE_SIZE;
	superBlock.s_size_block = BLOCK_SIZE;
	//Root directory and accounting file will use some inodes and blocks.
	superBlock.s_num_fblock = BLOCK_NUM - 2;
	superBlock.s_num_finode = INODE_NUM - 2;
	superBlock.special_stack[0] = 99;
	for (int i = 1; i < 50; i++)
	{
		superBlock.special_stack[i] = 49 - i;
	}
	superBlock.special_free = 47;
	//Write super block into file.
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock, sizeof(filsys), 1, fd);

	/*
	*	初始化位图
	*/
	unsigned short inode_bitmap[INODE_NUM];
	//Root directory and accounting file will use some inodes and blocks.
	memset(inode_bitmap, 0, INODE_NUM);
	inode_bitmap[0] = 1;
	inode_bitmap[1] = 1;
	//Write bitmaps into file.
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	//成组连接
	unsigned int stack[51];
	for (int i = 0; i < BLOCK_NUM / 50; i++)
	{
		memset(stack, 0, sizeof(stack));
		for (unsigned int j = 0; j < 50; j++)
		{
			stack[j] = (49 + i * 50) - j;
		}
		stack[0] = 49 + (i + 1) * 50;
		stack[50] = 49;
		fseek(fd, DATA_START + (49 + i * 50)*BLOCK_SIZE, SEEK_SET);
		fwrite(stack, sizeof(unsigned int) * 51, 1, fd);
	}
	memset(stack, 0, sizeof(stack));
	for (int i = 0; i < 12; i++)
	{
		stack[i] = 511 - i;
	}
	stack[0] = 0;
	stack[50] = 11;
	fseek(fd, DATA_START + 511 * BLOCK_SIZE, SEEK_SET);
	fwrite(stack, sizeof(unsigned int) * 51, 1, fd);

	fseek(fd, DATA_START + 49 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 99 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 149 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 199 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 249 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 299 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 349 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 399 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 449 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 499 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 511 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);

	/*
	*	创建根目录
	*/
	inode iroot_tmp;
	iroot_tmp.i_ino = 0;					//Identification
	iroot_tmp.di_number = 2;				//Associations: itself and accouting file
	iroot_tmp.di_mode = 0;					//0 stands for directory
	iroot_tmp.di_size = 0;					//"For directories, the value is 0."
	memset(iroot_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
	iroot_tmp.di_addr[0] = 0;				//Root directory is stored on 1st block. FFFFFF means empty.
	iroot_tmp.permission = MAX_OWNER_PERMISSION;
	iroot_tmp.di_grp = 1;
	iroot_tmp.di_uid = 0;					
	iroot_tmp.icount = 0;
	time_t t = time(0);
	strftime(iroot_tmp.time, sizeof(iroot_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	iroot_tmp.time[64] = 0;
	fseek(fd, INODE_START, SEEK_SET);
	fwrite(&iroot_tmp, sizeof(inode), 1, fd);
	directory droot_tmp;
	memset(droot_tmp.fileName, 0, sizeof(char) * DIRECTORY_NUM * FILE_NAME_LENGTH);
	memset(droot_tmp.inodeID, -1, sizeof(unsigned int) * DIRECTORY_NUM);
	strcpy(droot_tmp.fileName[0], ".");
	droot_tmp.inodeID[0] = 0;
	strcpy(droot_tmp.fileName[1], "..");
	droot_tmp.inodeID[1] = 0;
	strcpy(droot_tmp.fileName[2], "pw");
	droot_tmp.inodeID[2] = 1;
	fseek(fd, DATA_START, SEEK_SET);
	fwrite(&droot_tmp, sizeof(directory), 1, fd);

	/*
	*	创建用户文件
	*/
	inode iaccouting_tmp;
	iaccouting_tmp.i_ino = 1;					//Identification
	iaccouting_tmp.di_number = 1;				//Associations
	iaccouting_tmp.di_mode = 1;					//1 stands for file
	iaccouting_tmp.di_size = sizeof(userPsw);	//File size
	memset(iaccouting_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
	iaccouting_tmp.di_addr[0] = 1;				//Root directory is stored on 1st block.
	iaccouting_tmp.di_uid = 0;					
	iaccouting_tmp.di_grp = 1;
	iaccouting_tmp.permission = 320;
	iaccouting_tmp.icount = 0;
	t = time(0);
	strftime(iaccouting_tmp.time, sizeof(iaccouting_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	iaccouting_tmp.time[64] = 0;
	fseek(fd, INODE_START + INODE_SIZE, SEEK_SET);
	fwrite(&iaccouting_tmp, sizeof(inode), 1, fd);
	userPsw paccouting_tmp;
	memset(paccouting_tmp.userName, 0, sizeof(char) * USER_NAME_LENGTH * ACCOUNT_NUM);
	memset(paccouting_tmp.password, 0, sizeof(char) * USER_PASSWORD_LENGTH * ACCOUNT_NUM);
	//Only default user 'admin' is registered. Default password is 'admin'.
	strcpy(paccouting_tmp.userName[0], "admin");
	strcpy(paccouting_tmp.userName[1], "guest");
	strcpy(paccouting_tmp.password[0], "admin");
	strcpy(paccouting_tmp.password[1], "123456");
	//0 stands for super user. Other IDs are only used to identify users.
	for (unsigned short i = 0; i < ACCOUNT_NUM; i++)
	{
		paccouting_tmp.userID[i] = i;
	}
	paccouting_tmp.groupID[0] = 1;
	paccouting_tmp.groupID[1] = 2;
	fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
	fwrite(&paccouting_tmp, sizeof(userPsw), 1, fd);
	fclose(fd);
	printf("Format successfully!\n");
	return true;
};
/*
*	Initialization function of the file system. Open an existing file system
*	from 'fs.han'.
*
*	return: the function return true only when the file system has been
*			formatted and is complete.
*/
bool Mount()
{
	/*
	*	打开已有文件
	*/
	fd = fopen("./fs.han", "rb+");
	if (fd == NULL)
	{
		printf("Error: File system not found!\n");
		return false;
	}
	/*
	*	读超级块、位图、目录、用户文件
	*/
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock, sizeof(superBlock), 1, fd);

	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fread(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	fseek(fd, DATA_START, SEEK_SET);
	fread(&currentDirectory, sizeof(directory), 1, fd);

	fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
	fread(&users, sizeof(userPsw), 1, fd);

	return true;
};

/*
*	Log in function. Update user information by checking log in inputs.
*
*	return: the function return true only when log in process succeed.
*/
bool Login(const char* user, const char* password)
{
	//parameters check
	if (user == NULL || password == NULL)
	{
		printf("Error: User name or password illegal!\n\n");
		return false;
	}
	if (strlen(user) > USER_NAME_LENGTH || strlen(password) > USER_PASSWORD_LENGTH)
	{
		printf("Error: User name or password illegal!\n");
		return false;
	}

	//have logged in?
	if (userID != ACCOUNT_NUM)
	{
		printf("Login failed: User has been logged in. Please log out first.\n");
		return false;
	}

	//search the user in accouting file
	for (int i = 0; i < ACCOUNT_NUM; i++)
	{
		if (strcmp(users.userName[i], user) == 0)
		{
			//find the user and check password
			if (strcmp(users.password[i], password) == 0)
			{
				//Login successfully
				printf("Login successfully.\n");
				userID = users.userID[i];
				//make user's name, root user is special
				memset(userName, 0, USER_NAME_LENGTH + 6);
				if (userID == 0)
				{
					strcat(userName, "root ");
					strcat(userName, users.userName[i]);
					strcat(userName, "$");
				}
				else
				{
					strcat(userName, users.userName[i]);
					strcat(userName, "#");
				}

				return true;
			}
			else
			{
				//Password wrong
				printf("Login failed: Wrong password.\n");
				return false;
			}
		}
	}

	//User not found
	printf("Login failed: User not found.\n");
	return false;

};

/*
*	Log out function. Remove user's states.
*/
void Logout()
{
	//remove user's states
	userID = ACCOUNT_NUM;
	memset(&users, 0, sizeof(users));
	memset(userName, 0, 6 + USER_NAME_LENGTH);
	Mount();
};

/*
*	Create a new empty file with the specific file name.
*
*	return: the function return true only when the new file is successfully
*			created.
*/
bool CreateFile(const char* filename)
{
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return false;
	}

	if (superBlock.s_num_fblock <= 0 || superBlock.s_num_finode <= 0)
	{
		printf("File creation error: No valid spaces.\n");
		return false;
	}
	//Find new inode number and new block address
	int new_ino = 0;
	unsigned int new_block_addr = -1;
	for (; new_ino < INODE_NUM; new_ino++)
	{
		if (inode_bitmap[new_ino] == 0)
		{
			break;
		}
	}

	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strcmp(currentDirectory.fileName[i], filename) == 0)
		{
			inode* tmp_file_inode = new inode;
			int tmp_file_ino = currentDirectory.inodeID[i];
			fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
			fread(tmp_file_inode, sizeof(inode), 1, fd);
			if (tmp_file_inode->di_mode == 0) continue;
			else {
				printf("File creation error: File name '%s' has been used.\n", currentDirectory.fileName[i]);
				return false;
			}
		}
	}

	int itemCounter = 0;
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) > 0)
		{
			itemCounter++;
		}
	}
	if (itemCounter >= DIRECTORY_NUM)
	{
		printf("File creation error: Too many files or directories in current path.\n");
		return false;
	}

	inode ifile_tmp;
	ifile_tmp.i_ino = new_ino;				//Identification
	ifile_tmp.di_number = 1;				//Associations
	ifile_tmp.di_mode = 1;					//1 stands for file
	ifile_tmp.di_size = 0;					//New file is empty
	memset(ifile_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
	ifile_tmp.di_uid = userID;				//Current user id.
	ifile_tmp.di_grp = users.groupID[userID];//Current user group id
	ifile_tmp.permission = MAX_PERMISSION;
	ifile_tmp.icount = 0;
	time_t t = time(0);
	strftime(ifile_tmp.time, sizeof(ifile_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	ifile_tmp.time[64];
	fseek(fd, INODE_START + new_ino * INODE_SIZE, SEEK_SET);
	fwrite(&ifile_tmp, sizeof(inode), 1, fd);

	inode_bitmap[new_ino] = 1;
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	int pos_directory_inode = 0;
	pos_directory_inode = currentDirectory.inodeID[0]; //"."
	inode tmp_directory_inode;
	fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
	fread(&tmp_directory_inode, sizeof(inode), 1, fd);

	for (int i = 2; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) == 0)
		{
			strcat(currentDirectory.fileName[i], filename);
			currentDirectory.inodeID[i] = new_ino;
			break;
		}
	}
	fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);

	directory tmp_directory = currentDirectory;
	int tmp_pos_directory_inode = pos_directory_inode;
	while (true)
	{
		tmp_directory_inode.di_number++;
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
		if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
		{
			break;
		}
		tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fread(&tmp_directory, sizeof(directory), 1, fd);
	}

	superBlock.s_num_finode--;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	printf("Create a file successfully!\n");
	return true;
};

/*
*	Delete a file.
*
*	return: the function returns true only delete the file successfully.
*/
bool DeleteFile(const char* filename)
{
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return false;
	}

	int pos_in_directory = -1, tmp_file_ino;
	inode tmp_file_inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Delete error: File not found.\n");
			return false;
		}

		tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(&tmp_file_inode, sizeof(inode), 1, fd);
	} while (tmp_file_inode.di_mode == 0);

	if (userID == tmp_file_inode.di_uid)
	{
		if (!(tmp_file_inode.permission & OWN_X)) {
			printf("Delete error: Access deny.\n");
			return -1;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode.di_grp) {
		if (!(tmp_file_inode.permission & GRP_X)) {
			printf("Delete error: Access deny.\n");
			return -1;
		}
	}
	else {
		if (!(tmp_file_inode.permission & ELSE_X)) {
			printf("Delete error: Access deny.\n");
			return -1;
		}
	}
	if (tmp_file_inode.icount > 0) {
		tmp_file_inode.icount--;
		fseek(fd, INODE_START + tmp_file_inode.i_ino * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_file_inode, sizeof(inode), 1, fd);

		int pos_directory_inode = currentDirectory.inodeID[0];	//"."
		inode tmp_directory_inode;
		fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);

		memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
		currentDirectory.inodeID[pos_in_directory] = -1;
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fwrite(&currentDirectory, sizeof(directory), 1, fd);

		directory tmp_directory = currentDirectory;
		int tmp_pos_directory_inode = pos_directory_inode;
		while (true)
		{
			tmp_directory_inode.di_number--;
			fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
			fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
			if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
			{
				break;
			}
			tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
			fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
			fread(&tmp_directory_inode, sizeof(inode), 1, fd);
			fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
			fread(&tmp_directory, sizeof(directory), 1, fd);
		}
		printf("Delete a file successfully!\n");
		return true;
	}
	int tmp_fill[sizeof(inode)];
	memset(tmp_fill, 0, sizeof(inode));
	fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
	fwrite(&tmp_fill, sizeof(inode), 1, fd);

	inode_bitmap[tmp_file_ino] = 0;
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(&inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	for (int i = 0; i < NADDR - 2; i++)
	{
		if(tmp_file_inode.di_addr[i] != -1)
			recycle_block(tmp_file_inode.di_addr[i]);
		else break;
	}
	if (tmp_file_inode.di_addr[NADDR - 2] != -1) {
		unsigned int f1[128];
		fseek(fd, DATA_START + tmp_file_inode.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		for (int k = 0; k < 128; k++) {
			recycle_block(f1[k]);
		}
		recycle_block(tmp_file_inode.di_addr[NADDR - 2]);
	}

	int pos_directory_inode = currentDirectory.inodeID[0];	//"."
	inode tmp_directory_inode;
	fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
	fread(&tmp_directory_inode, sizeof(inode), 1, fd);

	memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
	currentDirectory.inodeID[pos_in_directory] = -1;
	fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);

	directory tmp_directory = currentDirectory;
	int tmp_pos_directory_inode = pos_directory_inode;
	while (true)
	{
		tmp_directory_inode.di_number--;
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
		if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
		{
			break;
		}
		tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fread(&tmp_directory, sizeof(directory), 1, fd);
	}

	superBlock.s_num_finode++;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	printf("Delete a file successfully!\n");
	return true;
}
/*
*	Open the specific file under current directory.
*
*	return: the function returns a pointer of the file's inode if the file is
*			successfully opened and NULL otherwise.
*/
inode* OpenFile(const char* filename)
{
	//文件名为空或者文件名大于文件名的最大长度
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)  
	{
		printf("Error: Illegal file name.\n");
		return NULL;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)//目录中文件的位置 小于 一个目录中子文件和子目录的最大数量16
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Open file error: File not found.\n");
			return NULL;
		}

		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];   //文件索引节点
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);    //SEEK_SET：文件开头   把fp指针移到离 文件开头 数据块起始位置
		fread(tmp_file_inode, sizeof(inode), 1, fd);    //将文件中二进制数据 读到 文件索引节点
	} while (tmp_file_inode->di_mode == 0);     //文件索引节点 表示类型是 directory
	printf("Open a file successfully!\n");
	return tmp_file_inode;
};

/*
*	Append a string "content" to the specific file.
*
*	return: the function returns the number of bytes it has writen or -1 when
*			some error occur.
*/
int Write(inode& ifile, const char* content)
{
	if (content == NULL)
	{
		printf("Error: No content.\n");
		return -1;
	}
	if (userID == ifile.di_uid)
	{
		if (!(ifile.permission & OWN_W)) {
			printf("Write error: Access deny.\n");
			return -1;
		}
	}
	else if (users.groupID[userID] == ifile.di_grp) {
		if (!(ifile.permission & GRP_W)) {
			printf("Write error: Access deny.\n");
			return -1;
		}
	}
	else {
		if (!(ifile.permission & ELSE_W)) {
			printf("Write error: Access deny.\n");
			return -1;
		}
	}

	int len_content = strlen(content);
	unsigned int new_file_length = len_content + ifile.di_size;
	if (new_file_length >= FILE_SIZE_MAX)
	{
		printf("Write error: File over length.\n");
		return -1;
	}

	unsigned int block_num;
	if (ifile.di_addr[0] == -1)block_num = -1;
	else
	{
		for (int i = 0; i < NADDR - 2; i++)
		{
			if (ifile.di_addr[i] != -1)
				block_num = ifile.di_addr[i];
			else break;
		}
		int f1[128];
		fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
		int num;
		if (ifile.di_size%BLOCK_SIZE == 0)
			num = ifile.di_size / BLOCK_SIZE;
		else num = ifile.di_size / BLOCK_SIZE + 1;
		if (num > 4 && num <=132)
		{
			fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
			fread(f1, sizeof(f1), 1, fd);
			block_num = f1[num - 4];
		}

	}
	int free_space_firstBlock = BLOCK_SIZE - ifile.di_size % BLOCK_SIZE;
	unsigned int num_block_needed;
	if (len_content - free_space_firstBlock > 0)
	{
		num_block_needed = (len_content - free_space_firstBlock) / BLOCK_SIZE + 1;
	}
	else
	{
		num_block_needed = 0;
	}
	if (num_block_needed > superBlock.s_num_fblock)
	{
		printf("Write error: No enough space available.\n");
		return -1;
	}

	if (ifile.di_addr[0] == -1)
	{
		find_free_block(block_num);
		ifile.di_addr[0] = block_num;
		fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
	}
	else
		fseek(fd, DATA_START + block_num * BLOCK_SIZE + ifile.di_size % BLOCK_SIZE, SEEK_SET);
	char data[BLOCK_SIZE];
	if (num_block_needed == 0)
	{
		fwrite(content, len_content, 1, fd);
		fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
		fread(data, sizeof(data), 1, fd);
		ifile.di_size += len_content;
	}
	else
	{
		fwrite(content, free_space_firstBlock, 1, fd);
		fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
		fread(data, sizeof(data), 1, fd);
		ifile.di_size += free_space_firstBlock;
	}

	char write_buf[BLOCK_SIZE];
	unsigned int new_block_addr = -1;
	unsigned int content_write_pos = free_space_firstBlock;
	if ((len_content + ifile.di_size) / BLOCK_SIZE + ((len_content + ifile.di_size) % BLOCK_SIZE == 0 ? 0 : 1) <= NADDR - 2) {
		for (int i = 0; i < num_block_needed; i++)
		{
			find_free_block(new_block_addr);
			if (new_block_addr == -1)return -1;
			for (int j = 0; j < NADDR - 2; j++)
			{
				if (ifile.di_addr[j] == -1)
				{
					ifile.di_addr[j] = new_block_addr;
					break;
				}
			}
			memset(write_buf, 0, BLOCK_SIZE);
			unsigned int tmp_counter = 0;
			for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
			{
				if (content[content_write_pos + tmp_counter] == '\0')
					break;
				write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
			}
			content_write_pos += tmp_counter;
			fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
			fwrite(write_buf, tmp_counter, 1, fd);
			fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
			fread(data, sizeof(data), 1, fd);
			ifile.di_size += tmp_counter;
		}
	}
	else if ((len_content+ifile.di_size)/BLOCK_SIZE+((len_content + ifile.di_size) % BLOCK_SIZE == 0 ? 0 : 1)> NADDR - 2) {
		for (int i = 0; i < NADDR - 2; i++)
		{
			if (ifile.di_addr[i] != -1)continue;

			memset(write_buf, 0, BLOCK_SIZE);
			new_block_addr = -1;

			find_free_block(new_block_addr);
			if (new_block_addr == -1)return -1;
			ifile.di_addr[i] = new_block_addr;
			unsigned int tmp_counter = 0;
			for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
			{
				if (content[content_write_pos + tmp_counter] == '\0') {
					break;
				}
				write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
			}
			content_write_pos += tmp_counter;
			fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
			fwrite(write_buf, tmp_counter, 1, fd);

			ifile.di_size += tmp_counter;
		}
		int cnt = 0;
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };

		new_block_addr = -1;
		find_free_block(new_block_addr);
		if (new_block_addr == -1)return -1;
		ifile.di_addr[NADDR - 2] = new_block_addr;
		for (int i = 0;i < BLOCK_SIZE / sizeof(unsigned int);i++)
		{
			new_block_addr = -1;
			find_free_block(new_block_addr);
			if (new_block_addr == -1)return -1;
			else
				f1[i] = new_block_addr;
		}
		fseek(fd, DATA_START + ifile.di_addr[4] * BLOCK_SIZE, SEEK_SET);
		fwrite(f1, sizeof(f1), 1, fd);
		bool flag = 0;
		for (int j = 0; j < BLOCK_SIZE / sizeof(int); j++) {
			fseek(fd, DATA_START + f1[j] * BLOCK_SIZE, SEEK_SET);
			unsigned int tmp_counter = 0;
			for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
			{
				if (content[content_write_pos + tmp_counter] == '\0') {
					flag = 1;
					break;
				}
				write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
			}
			content_write_pos += tmp_counter;
			fwrite(write_buf, tmp_counter, 1, fd);
			ifile.di_size += tmp_counter;
			if (flag == 1) break;
		}
	}
	time_t t = time(0);
	strftime(ifile.time, sizeof(ifile.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	ifile.time[64] = 0;
	fseek(fd, INODE_START + ifile.i_ino * INODE_SIZE, SEEK_SET);
	fwrite(&ifile, sizeof(inode), 1, fd);

	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(superBlock), 1, fd);

	return len_content;
};
/*
*	Print the string "content" in the specific file.
*
*	return: none
*
*/
void PrintFile(inode& ifile)
{
	if (userID == ifile.di_uid)
	{
		if (!(ifile.permission & OWN_R)) {
			printf("Read error: Access deny.\n");
			return;
		}
	}
	else if (users.groupID[userID] == ifile.di_grp) {
		if (!(ifile.permission & GRP_R)) {
			printf("Read error: Access deny.\n");
			return;
		}
	}
	else {
		if (!(ifile.permission & ELSE_R)) {
			printf("Read error: Access deny.\n");
			return;
		}
	}
	int block_num = ifile.di_size / BLOCK_SIZE + 1;
	int print_line_num = 0;		
	char stack[BLOCK_SIZE];
	if (block_num <= NADDR - 2)
	{
		for (int i = 0; i < block_num; i++)
		{
			fseek(fd, DATA_START + ifile.di_addr[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0')break;
				if (j % 16 == 0)
				{
					printf("\n");
					printf("%d\t", ++print_line_num);
				}
				printf("%c", stack[j]);
			}
		}
	}
	else if (block_num > NADDR - 2) {
		for (int i = 0; i < NADDR - 2; i++)
		{
			fseek(fd, DATA_START + ifile.di_addr[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0')break;
				if (j % 16 == 0)
				{
					printf("\n");
					printf("%d\t", ++print_line_num);
				}
				printf("%c", stack[j]);
			}
		}
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
		fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		for (int i = 0; i < block_num - (NADDR - 2); i++) {
			fseek(fd, DATA_START + f1[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0')break;
				if (j % 16 == 0)
				{
					printf("\n");
					printf("%d\t", ++print_line_num);
				}
				printf("%c", stack[j]);
			}
		}
	}
	printf("\n\n\n");
};

/*
*	Create a new drirectory only with "." and ".." items.
*
*	return: the function returns true only when the new directory is
*			created successfully.
*/
bool MakeDir(const char* dirname)
{
	if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal directory name.\n");
		return false;
	}

	if (superBlock.s_num_fblock <= 0 || superBlock.s_num_finode <= 0)
	{
		printf("File creation error: No valid spaces.\n");
		return false;
	}

	int new_ino = 0;
	unsigned int new_block_addr = 0;
	for (; new_ino < INODE_NUM; new_ino++)
	{
		if (inode_bitmap[new_ino] == 0)
		{
			break;
		}
	}
	find_free_block(new_block_addr);
	if (new_block_addr == -1) return false;
	if (new_ino == INODE_NUM || new_block_addr == BLOCK_NUM)
	{
		printf("File creation error: No valid spaces.\n");
		return false;
	}

	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strcmp(currentDirectory.fileName[i], dirname) == 0)
		{
			inode* tmp_file_inode = new inode;
			int tmp_file_ino = currentDirectory.inodeID[i];
			fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
			fread(tmp_file_inode, sizeof(inode), 1, fd);
			if (tmp_file_inode->di_mode == 1) continue;
			else {
				printf("File creation error: Directory name '%s' has been used.\n", currentDirectory.fileName[i]);
				return false;
			}
		}
	}

	int itemCounter = 0;
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) > 0)
		{
			itemCounter++;
		}
	}
	if (itemCounter >= DIRECTORY_NUM)
	{
		printf("File creation error: Too many files or directories in current path.\n");
		return false;
	}

	inode idir_tmp;
	idir_tmp.i_ino = new_ino;				//Identification
	idir_tmp.di_number = 1;					//Associations
	idir_tmp.di_mode = 0;					//0 stands for directory
	idir_tmp.di_size = sizeof(directory);	//"For directories, the value is 0."
	memset(idir_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
	idir_tmp.di_addr[0] = new_block_addr;
	idir_tmp.di_uid = userID;				
	idir_tmp.di_grp = users.groupID[userID];
	time_t t = time(0);
	strftime(idir_tmp.time, sizeof(idir_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	idir_tmp.time[64] = 0;
	idir_tmp.icount = 0;
	idir_tmp.permission = MAX_PERMISSION;
	fseek(fd, INODE_START + new_ino * INODE_SIZE, SEEK_SET);
	fwrite(&idir_tmp, sizeof(inode), 1, fd);

	directory tmp_dir;
	memset(tmp_dir.fileName, 0, sizeof(char) * DIRECTORY_NUM * FILE_NAME_LENGTH);
	memset(tmp_dir.inodeID, -1, sizeof(unsigned int) * DIRECTORY_NUM);
	strcpy(tmp_dir.fileName[0], ".");
	tmp_dir.inodeID[0] = new_ino;
	strcpy(tmp_dir.fileName[1], "..");
	tmp_dir.inodeID[1] = currentDirectory.inodeID[0];
	fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
	fwrite(&tmp_dir, sizeof(directory), 1, fd);

	inode_bitmap[new_ino] = 1;
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	int pos_directory_inode = 0;
	pos_directory_inode = currentDirectory.inodeID[0]; //"."
	inode tmp_directory_inode;
	fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
	fread(&tmp_directory_inode, sizeof(inode), 1, fd);

	for (int i = 2; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) == 0)
		{
			strcat(currentDirectory.fileName[i], dirname);
			currentDirectory.inodeID[i] = new_ino;
			break;
		}
	}
	fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);

	directory tmp_directory = currentDirectory;
	int tmp_pos_directory_inode = pos_directory_inode;
	while (true)
	{
		tmp_directory_inode.di_number++;
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
		if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
		{
			break;
		}
		tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fread(&tmp_directory, sizeof(directory), 1, fd);
	}

	superBlock.s_num_finode--;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	printf("Create a directory successfully!\n");
	return true;
};

/*
*	Delete a drirectory as well as all files and sub-directories in it.
*
*	return: the function returns true only when the directory as well
*			as all files and sub-directories in it is deleted successfully.
*/
bool RemoveDir(const char* dirname)
{
	if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal directory name.\n");
		return false;
	}

	int pos_in_directory = 0;
	int tmp_dir_ino;
	inode tmp_dir_inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], dirname) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Delete error: Directory not found.\n");
			return false;
		}

		tmp_dir_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
		fread(&tmp_dir_inode, sizeof(inode), 1, fd);
	} while (tmp_dir_inode.di_mode == 1);

	if (userID == tmp_dir_inode.di_uid)
	{
		if (!(tmp_dir_inode.permission & OWN_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else if (users.groupID[userID] == tmp_dir_inode.di_grp) {
		if (!(tmp_dir_inode.permission & GRP_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else {
		if (!(tmp_dir_inode.permission & ELSE_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}

	if (tmp_dir_inode.icount > 0) {
		tmp_dir_inode.icount--;
		fseek(fd, INODE_START + tmp_dir_inode.i_ino * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_dir_inode, sizeof(inode), 1, fd);

		int pos_directory_inode = currentDirectory.inodeID[0];	//"."
		inode tmp_directory_inode;
		fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);

		memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
		currentDirectory.inodeID[pos_in_directory] = -1;
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fwrite(&currentDirectory, sizeof(directory), 1, fd);

		directory tmp_directory = currentDirectory;
		int tmp_pos_directory_inode = pos_directory_inode;
		while (true)
		{
			tmp_directory_inode.di_number--;
			fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
			fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
			if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
			{
				break;
			}
			tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
			fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
			fread(&tmp_directory_inode, sizeof(inode), 1, fd);
			fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
			fread(&tmp_directory, sizeof(directory), 1, fd);
		}
		return true;
	}
	directory tmp_dir;
	fseek(fd, DATA_START + tmp_dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fread(&tmp_dir, sizeof(directory), 1, fd);

	inode tmp_sub_inode;
	char tmp_sub_filename[FILE_NAME_LENGTH];
	memset(tmp_sub_filename, 0, FILE_NAME_LENGTH);
	for (int i = 2; i < DIRECTORY_NUM; i++)
	{
		if (strlen(tmp_dir.fileName[i]) > 0)
		{
			strcpy(tmp_sub_filename, tmp_dir.fileName[i]);
			fseek(fd, INODE_START + tmp_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
			fread(&tmp_sub_inode, sizeof(inode), 1, fd);
			directory tmp_swp;
			tmp_swp = currentDirectory;
			currentDirectory = tmp_dir;
			tmp_dir = tmp_swp;
			if (tmp_sub_inode.di_mode == 1)
			{
				DeleteFile(tmp_sub_filename);
			}
			else if (tmp_sub_inode.di_mode == 0)
			{
				RemoveDir(tmp_sub_filename);
			}
			tmp_swp = currentDirectory;
			currentDirectory = tmp_dir;
			tmp_dir = tmp_swp;
		}
	}

	int tmp_fill[sizeof(inode)];
	memset(tmp_fill, 0, sizeof(inode));
	fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
	fwrite(&tmp_fill, sizeof(inode), 1, fd);

	inode_bitmap[tmp_dir_ino] = 0;
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(&inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
	//block bitmap
	for (int i = 0; i < (tmp_dir_inode.di_size / BLOCK_SIZE + 1); i++)
	{
		recycle_block(tmp_dir_inode.di_addr[i]);
	}

	int pos_directory_inode = currentDirectory.inodeID[0];	//"."
	inode tmp_directory_inode;
	fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
	fread(&tmp_directory_inode, sizeof(inode), 1, fd);

	memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
	currentDirectory.inodeID[pos_in_directory] = -1;
	fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * INODE_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);

	directory tmp_directory = currentDirectory;
	int tmp_pos_directory_inode = pos_directory_inode;
	while (true)
	{
		tmp_directory_inode.di_number--;
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
		if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
		{
			break;
		}
		tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fread(&tmp_directory, sizeof(directory), 1, fd);
	}

	superBlock.s_num_finode++;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	printf("Remove a directory successfully!\n");
	return true;
};

/*
*	Open a directory.
*
*	return: the function returns true only when the directory is
*			opened successfully.
*/
bool OpenDir(const char* dirname)
{
	if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal directory name.\n");
		return false;
	}

	int pos_in_directory = 0;
	inode tmp_dir_inode;
	int tmp_dir_ino;
	do {
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], dirname) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Delete error: Directory not found.\n");
			return false;
		}

		tmp_dir_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
		fread(&tmp_dir_inode, sizeof(inode), 1, fd);
	} while (tmp_dir_inode.di_mode == 1);
	if (userID == tmp_dir_inode.di_uid)
	{
		if (tmp_dir_inode.permission & OWN_X != OWN_X) {
			printf("Open dir error: Access deny.\n");
			return NULL;
		}
	}
	else if (users.groupID[userID] == tmp_dir_inode.di_grp) {
		if (tmp_dir_inode.permission & GRP_X != GRP_X) {
			printf("Open dir error: Access deny.\n");
			return NULL;
		}
	}
	else {
		if (tmp_dir_inode.permission & ELSE_X != ELSE_X) {
			printf("Open dir error: Access deny.\n");
			return NULL;
		}
	}

	directory new_current_dir;
	fseek(fd, DATA_START + tmp_dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fread(&new_current_dir, sizeof(directory), 1, fd);
	currentDirectory = new_current_dir;
	if (dirname[0] == '.' && dirname[1] == 0) {
		dir_pointer;
	}
	else if (dirname[0] == '.' && dirname[1] == '.' && dirname[2] == 0) {
		if (dir_pointer != 0) dir_pointer--;
	}
	else {
		for (int i = 0; i < 14; i++) {
			ab_dir[dir_pointer][i] = dirname[i];
		}
		dir_pointer++;
	}
	printf("Open a directory successfully!\n");
	return true;
};

/*
*	Print file and directory information under current directory.
*/
void List()
{
	printf("\n     name\tuser\tgroup\tinodeID\tIcount\tsize\tpermission\ttime\n");
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) > 0)
		{
			inode tmp_inode;
			fseek(fd, INODE_START + currentDirectory.inodeID[i] * INODE_SIZE, SEEK_SET);
			fread(&tmp_inode, sizeof(inode), 1, fd);

			const char* tmp_type = tmp_inode.di_mode == 0 ? "d" : "-";
			const char* tmp_user = users.userName[tmp_inode.di_uid];
			const int tmp_grpID = tmp_inode.di_grp;

			printf("%10s\t%s\t%d\t%d\t%d\t%u\t%s", currentDirectory.fileName[i], tmp_user, tmp_grpID, tmp_inode.i_ino, tmp_inode.icount, tmp_inode.di_size, tmp_type);
			for (int x = 8; x > 0; x--) {
				if (tmp_inode.permission & (1 << x)) {
					if ((x + 1) % 3 == 0) printf("r");
					else if ((x + 1) % 3 == 2) printf("w");
					else printf("x");
				}
				else printf("-");
			}
			if(tmp_inode.permission & 1) printf("x\t");
			else printf("-\t");
			printf("%s\n", tmp_inode.time);
		}
	}

	printf("\n\n");
}

/*
*	Print absolute directory.
*/
void Ab_dir() {
	for (int i = 0; i < dir_pointer; i++)
		printf("%s/", ab_dir[i]);
	printf("\n");
}

/*
*	Change file permission.
*/
void Chmod(char* filename) {
	printf("File or Dir?(0 stands for file, 1 for dir):");
	int tt;
	scanf("%d", &tt);
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return;
	}

	/*
	*	1. Check whether the file exists in current directory.
	*/
	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return;
		}

		/*
		*	2. Fetch inode and check whether it's a directory.
		*/
		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == tt);
	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	printf("Please input 0&1 string(1 stands for having, 0 for not)\n");
	printf("Format: rwxrwxrwx\n");
	char str[10];
	scanf("%s", &str);
	int temp = 0;
	for (int i = 0; i < 8; i++) {
		if (str[i] == '1')
			temp += 1 << (8 - i);
	}
	if (str[8] == '1') temp += 1;
	tmp_file_inode->permission = temp;
	fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);
	printf("Change file permission successfully!\n");
	return;
}

/*
*	Change file' owner.
*/
void Chown(char* filename) {
	printf("File or Dir?(0 stands for file, 1 for dir):");
	int tt;
	scanf("%d", &tt);
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return;
		}

		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == tt);

	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}

	printf("Please input user name:");
	char str[USER_NAME_LENGTH];
	int i;
	scanf("%s", str);
	for (i = 0; i < ACCOUNT_NUM; i++) {
		if (strcmp(users.userName[i], str) == 0) break;
	}
	if (i == ACCOUNT_NUM) {
		printf("Error user!\n");
		return;
	}
	tmp_file_inode->di_uid = i;
	fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);
	printf("Change file's owner successfully!\n");
	return;
}

/*
*	Change file' group.
*/
void Chgrp(char* filename) {
	printf("File or Dir?(0 stands for file, 1 for dir):");
	int tt;
	scanf("%d", &tt);
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return;
		}

		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == tt);

	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}

	printf("Please input group number:");
	int ttt, i;
	scanf("%d", &ttt);
	for (i = 0; i < ACCOUNT_NUM; i++) {
		if (users.groupID[i] == ttt) break;
	}
	if (i == ACCOUNT_NUM) {
		printf("Error user!\n");
		return;
	}
	tmp_file_inode->di_grp = ttt;
	fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);
	printf("Change file's group successfully!\n");
	return;
}

/*
*	Change password.
*/
void Passwd() {
	printf("Please input old password:");
	char str[USER_PASSWORD_LENGTH+1];
	scanf("%s", str);
	str[USER_PASSWORD_LENGTH] = 0;
	if (strcmp(users.password[userID], str) == 0) {
		printf("Please input a new password:");
		scanf("%s", str);
		if (strcmp(users.password[userID], str) == 0) {
			printf("Password doesn't change!\n");
		}
		else {
			strcpy(users.password[userID], str);
			//Write
			fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
			fwrite(&users, sizeof(users), 1, fd);
			printf("Success\n");
		}
	}
	else {
		printf("Password Error!!!\n");
	}
	printf("Change password successfully!\n");
}

/*
*	User Management.
*/
void User_management() {
	if (userID != 0) {
		printf("Only administrator can manage users!\n");
		return;
	}
	printf("Welcome to user management!\n");
	char c, d;
	scanf("%c", &c);
	while (c != '0') {
		printf("\n1.View 2.Create 3.Remove 0.Save & Exit\n");
		scanf("%c", &c);
		switch (c) {
		case '1':
			for (int i = 0; i < ACCOUNT_NUM; i++) {
				if (users.userName[i][0] != 0)
					printf("%d\t%s\t%d\n", users.userID[i], users.userName[i], users.groupID[i]);
				else break;
			}
			scanf("%c", &c);
			break;
		case '2':
			int i;
			for (i = 0; i < ACCOUNT_NUM; i++) {
				if (users.userName[i][0] == 0) break;
			}
			if (i == ACCOUNT_NUM) {
				printf("Users Full!\n");
				break;
			}
			printf("Please input username:");
			char str[USER_NAME_LENGTH];
			scanf("%s", str);
			for (i = 0; i < ACCOUNT_NUM; i++) {
				if (strcmp(users.userName[i], str) == 0) {
					printf("Already has same name user!\n");
				}
				if (users.userName[i][0] == 0) {
					strcpy(users.userName[i], str);
					printf("Please input password:");
					scanf("%s", str);
					strcpy(users.password[i], str);
					printf("Please input group ID:");
					int t;
					scanf("%d", &t);
					scanf("%c", &c);
					if (t > 0) {
						users.groupID[i] = t;
						printf("Success!\n");
					}
					else {
						printf("Insert Error!\n");
						strcpy(users.userName[i], 0);
						strcpy(users.password[i], 0);
					}
					break;
				}
			}
			break;
		case '3':
			printf("Please input userID:");
			int tmp;
			scanf("%d", &tmp);scanf("%c", &c);
			if (strcmp(users.userName[tmp], "") == 0) {
				printf("Remove Error!\n");
			}
			else {
				for (int j = tmp; j < ACCOUNT_NUM - 1; j++) {
					strcpy(users.userName[j], users.userName[j + 1]);
					strcpy(users.password[j], users.password[j + 1]);
					users.groupID[j] = users.groupID[j + 1];
				}
				printf("Success!\n");
			}
			break;
		}
	}
	fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
	fwrite(&users, sizeof(users), 1, fd);
}

/*
*	Rename file/dir.
*/
void Rename(char* filename) {
	printf("File or Dir?(0 stands for file, 1 for dir):");
	int tt;
	scanf("%d", &tt);
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return;
		}

		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == tt);

	printf("Please input new file name:");
	char str[15];
	scanf("%s", str);
	str[14] = 0;
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (currentDirectory.inodeID[i] == tmp_file_inode->i_ino)
		{
			strcpy(currentDirectory.fileName[i], str);
			break;
		}
	}
	//write
	fseek(fd, DATA_START + tmp_file_inode->di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);
	printf("Rename successfully!\n");
	return;
}

/*
*	Link a file.
*/
bool ln(char* filename)
{

	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return false;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return false;
		}

		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
	} while (tmp_file_inode->di_mode == 0);

	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	//get absolute path
	char absolute[1024];
	int path_pos = 0;
	printf("Input absolute path(end with '#'):");
	scanf("%s", absolute);
	//get directory inode
	char dirname[14];
	int pos_dir = 0;
	bool root = false;
	inode dir_inode;
	directory cur_dir;
	int i;
	for (i = 0; i < 5; i++)
	{
		dirname[i] = absolute[i];
	}
	dirname[i] = 0;
	if (strcmp("root/", dirname) != 0)
	{
		printf("path error!\n");
		return false;
	}
	fseek(fd, INODE_START, SEEK_SET);
	fread(&dir_inode, sizeof(inode), 1, fd);
	for (int i = 5; absolute[i] != '#'; i++)
	{
		if (absolute[i] == '/')
		{
			dirname[pos_dir++] = 0;
			pos_dir = 0;
			fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
			fread(&cur_dir, sizeof(directory), 1, fd);
			int i;
			for (i = 0; i < DIRECTORY_NUM; i++)
			{
				if (strcmp(cur_dir.fileName[i], dirname) == 0)
				{
					fseek(fd, INODE_START + cur_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
					fread(&dir_inode, sizeof(inode), 1, fd);
					if (dir_inode.di_mode == 0)break;
				}
			}
			if (i == DIRECTORY_NUM)
			{
				printf("path error!\n");
				return false;
			}
		}
		else
			dirname[pos_dir++] = absolute[i];
	}
	//update this directory
	fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fread(&cur_dir, sizeof(directory), 1, fd);
	for (i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(cur_dir.fileName[i]) == 0)
		{
			strcat(cur_dir.fileName[i], filename);
			cur_dir.inodeID[i] = tmp_file_inode->i_ino;
			break;
		}
	}
	if (i == DIRECTORY_NUM)
	{
		printf("No value items!\n");
		return false;
	}
	fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&cur_dir, sizeof(directory), 1, fd);
	dir_inode.di_number++;
	tmp_file_inode->icount++;
	fseek(fd, INODE_START + tmp_file_inode->i_ino*INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);

	printf("Link a file successfully!\n");
	return true;
}

/*
*	File copy.
*/
bool Copy(char* filename, inode*& currentInode) {
	currentInode = OpenFile(filename);
	int block_num = currentInode->di_size / BLOCK_SIZE + 1;
	char stack[BLOCK_SIZE];
	char str[100000];
	int cnt = 0;
	if (block_num <= NADDR - 2)
	{
		for (int i = 0; i < block_num; i++)
		{
			if (currentInode->di_addr[i] == -1) break;
			fseek(fd, DATA_START + currentInode->di_addr[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0') {
					str[cnt] = 0;
					break;
				}
				str[cnt++] = stack[j];
			}
		}
	}
	else if (block_num > NADDR - 2) {
		//direct addressing
		for (int i = 0; i < NADDR - 2; i++)
		{
			fseek(fd, DATA_START + currentInode->di_addr[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0') {
					str[cnt] = 0;
					break;
				}
				str[cnt++] = stack[j];
			}
		}

		//first indirect addressing
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
		fseek(fd, DATA_START + currentInode->di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		for (int i = 0; i < block_num - (NADDR - 2); i++) {
			fseek(fd, DATA_START + f1[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0') {
					str[cnt] = 0;
					break;
				}
				str[cnt++] = stack[j];
			}
		}
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return false;
		}

		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == 0);

	//Access check

	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	//get absolute path
	char absolute[1024];
	int path_pos = 0;
	printf("Input absolute path(end with '#'):");
	scanf("%s", absolute);
	//get directory inode
	char dirname[14];
	int pos_dir = 0;
	bool root = false;
	inode dir_inode;
	directory cur_dir;
	int i;
	for (i = 0; i < 5; i++)
	{
		dirname[i] = absolute[i];
	}
	dirname[i] = 0;
	if (strcmp("root/", dirname) != 0)
	{
		printf("path error!\n");
		return false;
	}
	fseek(fd, INODE_START, SEEK_SET);
	fread(&dir_inode, sizeof(inode), 1, fd);
	for (int i = 5; absolute[i] != '#'; i++)
	{
		if (absolute[i] == '/')
		{
			dirname[pos_dir++] = 0;
			pos_dir = 0;
			fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
			fread(&cur_dir, sizeof(directory), 1, fd);
			int i;
			for (i = 0; i < DIRECTORY_NUM; i++)
			{
				if (strcmp(cur_dir.fileName[i], dirname) == 0)
				{
					fseek(fd, INODE_START + cur_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
					fread(&dir_inode, sizeof(inode), 1, fd);
					if (dir_inode.di_mode == 0)break;
				}
			}
			if (i == DIRECTORY_NUM)
			{
				printf("path error!\n");
				return false;
			}
		}
		else
			dirname[pos_dir++] = absolute[i];
	}
	//update this directory
	fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fread(&cur_dir, sizeof(directory), 1, fd);
	for (i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(cur_dir.fileName[i]) == 0)
		{
			strcat(cur_dir.fileName[i], filename);
			int new_ino = 0;
			for (; new_ino < INODE_NUM; new_ino++)
			{
				if (inode_bitmap[new_ino] == 0)
				{
					break;
				}
			}
			inode ifile_tmp;
			ifile_tmp.i_ino = new_ino;				//Identification
			ifile_tmp.icount = 0;
			ifile_tmp.di_uid = tmp_file_inode->di_uid;
			ifile_tmp.di_grp = tmp_file_inode->di_grp;
			ifile_tmp.di_mode = tmp_file_inode->di_mode;
			memset(ifile_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
			ifile_tmp.di_size = 0;
			ifile_tmp.permission = tmp_file_inode->permission;
			time_t t = time(0);
			strftime(ifile_tmp.time, sizeof(ifile_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
			cur_dir.inodeID[i] = new_ino;
			Write(ifile_tmp, str);
			//Update bitmaps
			inode_bitmap[new_ino] = 1;
			fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
			fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
			superBlock.s_num_finode--;
			fseek(fd, BLOCK_SIZE, SEEK_SET);
			fwrite(&superBlock, sizeof(filsys), 1, fd);
			break;
		}
	}
	if (i == DIRECTORY_NUM)
	{
		printf("No value iterms!\n");
		return false;
	}
	fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&cur_dir, sizeof(directory), 1, fd);
	dir_inode.di_number++;
	fseek(fd, INODE_START + tmp_file_inode->i_ino*INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);
	printf("Copy a file successfully!\n");
	return true;
}

int main()
{
	
	memset(ab_dir, 0, sizeof(ab_dir));
	dir_pointer = 0;
	//Check file system has been formatted or not.
	FILE* fs_test = fopen("fs.han", "r");
	if (fs_test == NULL)
	{
		printf("File system not found... Format file system first...\n\n");
		Format();
	}
	Sys_start();
	ab_dir[dir_pointer][0] = 'r';ab_dir[dir_pointer][1] = 'o';ab_dir[dir_pointer][2] = 'o';ab_dir[dir_pointer][3] = 't';ab_dir[dir_pointer][4] = '\0';
	dir_pointer++;
	//First log in
	char tmp_userName[USER_NAME_LENGTH];
	char tmp_userPassword[USER_PASSWORD_LENGTH * 5];

	do {
		memset(tmp_userName, 0, USER_NAME_LENGTH);
		memset(tmp_userPassword, 0, USER_PASSWORD_LENGTH * 5);

		printf("User log in\n\n");
		printf("User name:\t");
		scanf("%s", tmp_userName);
		printf("Password:\t");
		char c;
		scanf("%c", &c);
		int i = 0;
		while (1) {
			char ch;
			ch = getch();
			if (ch == '\b') {
				if (i != 0) {
					printf("\b \b");
					i--;
				}
				else {
					tmp_userPassword[i] = '\0';
				}
			}
			else if (ch == '\r') {
				tmp_userPassword[i] = '\0';
				printf("\n\n");
				break;
			}
			else {
				putchar('*');
				tmp_userPassword[i++] = ch;
			}
		}
		//scanf("%s", tmp_userPassword);
	} while (Login(tmp_userName, tmp_userPassword) != true);

	//User's mode of file system
	inode* currentInode = new inode;

	CommParser(currentInode);

	return 0;
}
//system start
void Sys_start() {
	//Install file system
	Mount();
	printf("\t\t \033[5m ******************************************************************************\n");
	printf("\t\t  \033[5m*                                                                            *\n");
	printf("\t\t  * \033[0m  \033[32m  *********   *********   ***          **********       *********   \033[0m    \033[5m *\n");
	printf("\t\t  * \033[0m  \033[32m  *********   *********   ***          **********     ************  \033[0m    \033[5m *\n");
	printf("\t\t  * \033[0m  \033[32m  ***            ***      ***          ***           ***            \033[0m    \033[5m *\n");
	printf("\t\t  * \033[0m  \033[32m  ***            ***      ***          ***           ***            \033[0m   \033[5m  *\n");
	printf("\t\t  * \033[0m  \033[32m  *********      ***      ***          **********      ***********  \033[0m    \033[5m *\n");
	printf("\t\t  *  \033[0m \033[32m  ***            ***      ***          ***                      *** \033[0m    \033[5m *\n");
	printf("\t\t  *  \033[0m \033[32m  ***            ***      ***          ***                      *** \033[0m   \033[5m  *\n");
	printf("\t\t  *  \033[0m \033[32m  ***         *********   ***********  **********      ***********  \033[0m   \033[5m  *\n");
	printf("\t\t  *  \033[0m \033[32m  ***         *********   ***********  ***********     *********    \033[0m    \033[5m *\n");
	printf("\t\t  *                                                                            *\n");
	printf("\t\t  ******************************************************************************\033[0m\n");
}
/*
*	Recieve commands from console and call functions accordingly.
*
*	para cuurentInode: a global inode used for 'open' and 'write'
*/
void CommParser(inode*& currentInode)
{
	char para1[11];
	char para2[1024];
	bool flag = false;
	//Recieve commands
	while (true)
	{
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
		fseek(fd, DATA_START + 8 * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		memset(para1, 0, 11);
		memset(para2, 0, 1024);

		printf("%s>", userName);
		scanf("%s", para1);
		para1[10] = 0;			//security protection

		//Choose function
		//Print current directory
		if (strcmp("ls", para1) == 0)
		{
			flag = false;
			List();
		}
		else if (strcmp("cp", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection
			Copy(para2, currentInode);
		}
		else if (strcmp("mv", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection
			Rename(para2);
		}
		else if (strcmp("pwd", para1) == 0) {
			flag = false;
			Ab_dir();
		}
		else if (strcmp("passwd", para1) == 0) {
			flag = false;
			Passwd();
		}
		else if (strcmp("chmod", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			Chmod(para2);
		}
		else if (strcmp("chown", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			Chown(para2);
		}
		else if (strcmp("chgrp", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			Chgrp(para2);
		}
		else if (strcmp("info", para1) == 0) {
			printf("System Info:\nTotal Block:%d\nFree Block:%d\nTotal Inode:%d\nFree Inode:%d\n\n", superBlock.s_num_block, superBlock.s_num_fblock, superBlock.s_num_inode, superBlock.s_num_finode);
			for (int i = 0; i < 50; i++)
			{
				if (i>superBlock.special_free)printf("-1\t");
				else printf("%d\t", superBlock.special_stack[i]);
				if (i % 10 == 9)printf("\n");
			}
			printf("\n\n");
		}
		//Create file
		else if (strcmp("create", para1) == 0)
		{
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			CreateFile(para2);
		}
		//Delete file
		else if (strcmp("rm", para1) == 0)
		{
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			DeleteFile(para2);
		}
		//Open file
		else if (strcmp("open", para1) == 0){
			if (flag == false) {
				flag = true;
				scanf("%s", para2);
				para2[1023] = 0;	//security protection
				currentInode = OpenFile(para2);
			}
			else printf("You have opened a file!\n");
		}
		//Write file
		else if (strcmp("write", para1) == 0 && flag){
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			Write(*currentInode, para2);
		}
		//Read file
		else if (strcmp("cat", para1) == 0 && flag) {
			PrintFile(*currentInode);
		}
		//Close file
		else if (strcmp("close", para1) == 0) {
			if (flag == true) {
				printf("Close successfully!\n");
				flag = false;
			}
			else {
				printf("You have never opened a file!\n");
			}
		}
		//Open a directory
		else if (strcmp("cd", para1) == 0){
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			OpenDir(para2);
		}
		//Create dirctory
		else if (strcmp("mkdir", para1) == 0){
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			MakeDir(para2);
		}
		//Delete directory
		else if (strcmp("rmdir", para1) == 0){
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			RemoveDir(para2);
		}
		//Log out
		else if (strcmp("logout", para1) == 0){
			flag = false;
			Logout();
			char tmp_userName[USER_NAME_LENGTH], tmp_userPassword[USER_PASSWORD_LENGTH * 5];
			do {
				memset(tmp_userName, 0, USER_NAME_LENGTH);
				memset(tmp_userPassword, 0, USER_PASSWORD_LENGTH * 5);

				printf("User log in\n\n");
				printf("User name:\t");
				scanf("%s", tmp_userName);
				printf("Password:\t");
				char c;
				scanf("%c", &c);
				int i = 0;
				while (1) {
					char ch;
					ch = getch();
					if (ch == '\b') {
						if (i != 0) {
							printf("\b \b");
							i--;
						}
						else {
							tmp_userPassword[i] = '\0';
						}
					}
					else if (ch == '\r') {
						tmp_userPassword[i] = '\0';
						printf("\n\n");
						break;
					}
					else {
						putchar('*');
						tmp_userPassword[i++] = ch;
					}
				}

				//scanf("%s", tmp_userPassword);
			} while (Login(tmp_userName, tmp_userPassword) != true);

			//User's mode of file system
			inode* currentInode = new inode;
		}
		else if (strcmp("ln", para1) == 0)
		{
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			ln(para2);
		}
		//Log in
		else if (strcmp("su", para1) == 0){
			Logout();
			flag = false;
			char para3[USER_PASSWORD_LENGTH * 5];
			memset(para3, 0, USER_PASSWORD_LENGTH + 1);

			scanf("%s", para2);
			para2[1023] = 0;	//security protection
			//scanf("%s", para3);
			printf("password: ");
			char c;
			scanf("%c", &c);
			int i = 0;
			while (1) {
				char ch;
				ch = getch();
				if (ch == '\b') {
					if (i != 0) {
						printf("\b \b");
						i--;
					}
				}
				else if (ch == '\r') {
					para3[i] = '\0';
					printf("\n\n");
					break;
				}
				else {
					putchar('*');
					para3[i++] = ch;
				}
			}
			para3[USER_PASSWORD_LENGTH] = 0;	//security protection

			Login(para2, para3);
		}
		else if (strcmp("Muser", para1) == 0) {
			flag = false;
			User_management();
		}
		//Exit
		else if (strcmp("exit", para1) == 0){
			flag = false;
			break;
		}
		//Error or help
		else{
			flag = false;
			Help();
		}
	}
};

/*
*	Print all commands help information when 'help' is
*	called or input error occurs.
*/
void Help(){
	printf("System command:\n");
	printf("\t01.\33[36mExit system....................................\33[5m\33[3m\33[37m(exit)\33[0m\n");
	printf("\t02.\33[36mShow help information..........................\33[5m\33[3m\33[37m(help)\33[0m\n");
	printf("\t03.\33[36mShow current directory..........................\33[5m\33[3m\33[37m(pwd)\33[0m\n");
	printf("\t04.\33[36mFile list of current directory...................\33[5m\33[3m\33[37m(ls)\33[0m\n");
	printf("\t05.\33[36mEnter the specified directory..........\33[5m\33[3m\33[37m(cd + dirname)\33[0m\n");
	printf("\t06.\33[36mReturn last directory.........................\33[5m\33[3m\33[37m(cd ..)\33[0m\n");
	printf("\t07.\33[36mCreate a new directory..............\33[5m\33[3m\33[37m(mkdir + dirname)\33[0m\n");
	printf("\t08.\33[36mDelete the directory................\33[5m\33[3m\33[37m(rmdir + dirname)\33[0m\n");
	printf("\t09.\33[36mCreate a new file....................\33[5m\33[3m\33[37m(create + fname)\33[0m\n");
	printf("\t10.\33[36mOpen a file............................\33[5m\33[3m\33[37m(open + fname)\33[0m\n");
	printf("\t11.\33[36mClose a file..................................\33[5m\33[3m\33[37m(close)\33[0m\n");
	printf("\t12.\33[36mRead the file...................................\33[5m\33[3m\33[37m(cat)\33[0m\n");
	printf("\t13.\33[36mWrite the file....................\33[5m\33[3m\33[37m(write + <content>)\33[0m\n");
	printf("\t14.\33[36mCopy a file..............................\33[5m\33[3m\33[37m(cp + fname)\33[0m\n");
	printf("\t15.\33[36mDelete a file............................\33[5m\33[3m\33[37m(rm + fname)\33[0m\n");
	printf("\t16.\33[36mSystem information view........................\33[5m\33[3m\33[37m(info)\33[0m\n");
	printf("\t17.\33[36mClose the current user.......................\33[5m\33[3m\33[37m(logout)\33[0m\n");
	printf("\t18.\33[36mChange the current user...............\33[5m\33[3m\33[37m(su + username)\33[0m\n");
	printf("\t19.\33[36mChange the mode of a file.............\33[5m\33[3m\33[37m(chmod + fname)\33[0m\n");
	printf("\t20.\33[36mChange the user of a file.............\33[5m\33[3m\33[37m(chown + fname)\33[0m\n");
	printf("\t21.\33[36mChange the group of a file............\33[5m\33[3m\33[37m(chgrp + fname)\33[0m\n");
	printf("\t22.\33[36mRename a file............................\33[5m\33[3m\33[37m(mv + fname)\33[0m\n");
	printf("\t23.\33[36mlink.....................................\33[5m\33[3m\33[37m(ln + fname)\33[0m\n");
	printf("\t24.\33[36mChange password..............................\33[5m\33[3m\33[37m(passwd)\33[0m\n");
	printf("\t25.\33[36mUser Management Menu..........................\33[5m\33[3m\33[37m(Muser)\33[0m\n");
};
