实现功能
01.Exit system....................................(exit)
02.Show help information..........................(help)
03.Show current directory..........................(pwd)
04.File list of current directory...................(ls)
05.Enter the specified directory..........(cd + dirname)
06.Return last directory.........................(cd ..)
07.Create a new directory..............(mkdir + dirname)
08.Delete the directory................(rmdir + dirname)
09.Create a new file....................(create + fname)
10.Open a file............................(open + fname)
11.Close a file....................................(close)
12.Read the file...................................(cat)
13.Write the file.....................(write + contents)
14.Copy a file..............................(cp + fname)
15.Delete a file............................(rm + fname)
16.System information view........................(info)
17.Close the current user.......................(logout)
18.Change the current user...............(su + username)
19.Change the mode of a file.............(chmod + fname)
20.Change the user of a file.............(chown + fname)
21.Change the group of a file............(chgrp + fname)
22.Rename a file............................(mv + fname)
23.link.....................................(ln + fname)
24.Change password..............................(passwd)
25.User Management Menu..........................(Muser)
注：该代码中cat指令与write指令与要求中有所不同，需要先对文件执行open指令后才可以读写。
注：初始化的时候会调用格式化，用户自带两个用户账号密码为admin、admin/guest、123456

