/*
 * 
 *  Copyright ( c ) 2012
 *  name : mhogo mchungu 
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  ( at your option ) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <fcntl.h>
#include "includes.h"
#include <errno.h>

int zuluCryptSecureOpenFile( const char * path,int * fd,string_t * file,uid_t uid )
{
	int st ;
	int f = -1 ;
	uid_t org = geteuid() ;
	char * dev ;
	seteuid( uid ) ;
	f = open( path,O_RDONLY ) ;
	if( f != -1 ){
		dev = zuluCryptGetFileNameFromFileDescriptor( f ) ;
		*file = StringInherit( &dev ) ;
		*fd = f ;
		st = 1 ;
	}else{
		st = 0  ;
	}
	seteuid( org ) ;
	return st ;
}

int zuluCryptGetDeviceFileProperties( const char * file,int * fd,char ** dev,uid_t uid )
{
	int st = 100 ;
	int xt = 0 ;
	int lfd ;
	
	string_t st_dev = StringVoid ;
	
	struct stat stat_st ;
	struct stat stat_st_1 ;
	/*
	 * try to open the device with user privileges
	 */
	seteuid( uid ) ;
	
	*dev = NULL ;
	
	*fd = open( file,O_RDONLY ) ;
	
	if( *fd != -1 ){
		fstat( *fd,&stat_st ) ;
		fcntl( *fd,F_SETFD,FD_CLOEXEC ) ;
		/*
		 * A user has access to the device.They should get here only with paths to files they have access to. 
		 * Allow access to files only
		 */
		if( S_ISREG( stat_st.st_mode ) ){
			/*
			 * we can open file in read mode,let see if we can in write mode too 
			 */
			lfd = open( file,O_RDWR ) ;
			if( lfd != -1 ){
				/*
				 * we can open the file in read write mode
				 */
				fstat( lfd,&stat_st_1 ) ;
				fcntl( lfd,F_SETFD,FD_CLOEXEC ) ;
				
				/*
				 * check to make sure the file is got earlier is the same one we got now.
				 * ie check to make sure the file wasnt swapped btw calls.
				 */
				if( stat_st.st_dev == stat_st_1.st_dev && stat_st.st_ino == stat_st_1.st_ino ){
					seteuid( 0 ) ;
					/*
					 * zuluCryptAttachLoopDeviceToFileUsingFileDescriptor() is defined in ./create_loop_device.c
					 */
					xt = zuluCryptAttachLoopDeviceToFileUsingFileDescriptor( lfd,O_RDWR,&st_dev ) ;
					seteuid( uid ) ;
					close( *fd ) ;
					*fd = lfd ;
					*dev = StringDeleteHandle( &st_dev ) ;
				}
			}else{
				/*
				 * we can not open the file in read write mode
				 */
				seteuid( 0 ) ;
				/*
				 * zuluCryptAttachLoopDeviceToFileUsingFileDescriptor() is defined in ./create_loop_device.c
				 */
				xt = zuluCryptAttachLoopDeviceToFileUsingFileDescriptor( *fd,O_RDONLY,&st_dev ) ;
				seteuid( uid ) ;
				*dev = StringDeleteHandle( &st_dev ) ;
			}
			
			if( xt != 1 ){
				st = 100 ;
				close( *fd ) ;
				*fd = -1 ;
			}else{
				st = 0 ;
			}
		}else{
			if( S_ISBLK( stat_st.st_mode ) ){
				if( uid == 0 ) {
					/*
					 * we got a block device and we are root,accept it
					 */
					*dev = zuluCryptGetFileNameFromFileDescriptor( *fd ) ;
					st = 0 ;
				}else{
					/*
					 * normal user has access to block device,it could be a writeble cdrom,accept it only 
					 * if its in /dev/ but not in /dev/shm
					 */
					*dev = zuluCryptGetFileNameFromFileDescriptor( *fd ) ;
					if( *dev != NULL ){
						if( strncmp( *dev,"/dev/shm/",9 ) == 0 ){
							st = 4 ;
						}else if( strncmp( *dev,"/dev/",5 ) == 0 ){
							st = 0 ;
						}
					}
				}
			}else if( S_ISDIR( stat_st.st_mode ) ){
				st = 2 ;
			}else{
				/*
				 * whatever it is,it cant be good,reject it
				 */
				st = 100 ;
			}
			close( *fd ) ;
			*fd = -1 ;
		}
	}else{
		/*
		 * failed to open it with normal user privileges,try to open it with root's
		 */
		
		seteuid( 0 ) ;
		
		*fd = open( file,O_RDONLY ) ;
		
		if( *fd != -1 ){
			fstat( *fd,&stat_st ) ;
			/*
			 * zuluCryptGetFileNameFromFileDescriptor() is defined in ./create_loop_device.c
			 */
			*dev = zuluCryptGetFileNameFromFileDescriptor( *fd ) ;
			/*
			 * A user has access to the device.
			 * we close the file when we are done examining them because they can not be moved under us and we dont have to
			 * hold on to them.Besides,we cant even if we want to as cryptsetup will demand exclusive access to them. 
			 */
			if( S_ISBLK( stat_st.st_mode ) ){
				if( *dev != NULL ){
					if( strncmp( *dev,"/dev/shm/",9 ) == 0 ){
						/*
						* we do not support this path
						*/
						st = 1 ;
					}else if( strncmp( *dev,"/dev/",5 ) == 0 ){
						/*
						* got the block device we want,accept it
						*/
						st = 0 ;
					}else{
						/*
						* reject others
						*/
						st = 100 ;
					}
				}
			}else if( S_ISDIR( stat_st.st_mode ) ){
				st = 2 ;
			}else{
				/*
				 * whatever it is,it cant be good,reject it
				 */
				st = 100 ;
			}
			close( *fd ) ;
			*fd = -1 ;
		}else{
			/*
			 * invalid path or something i dont know,reject
			 */
			st = 100 ;
		}
		
		seteuid( uid ) ;
	}
	return st ;
}