
module temp
   type real_num
      real :: x
   end type

   interface operator (.add.)
      module procedure real_add
   end interface

 ! Unclear why the subroutine definition must be in the interface block
 !  interface assignment(=)
 !     subroutine real_sub(a,b)
 !        integer, intent(out) :: a
 !        logical, intent(in)  :: b
 !     end subroutine real_sub
 !  end interface

   contains
      function real_add(a,b)
         integer :: real_add
         integer, intent(in) :: a,b
         real_add = 1
      end function real_add


end module

