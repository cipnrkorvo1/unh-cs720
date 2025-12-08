export outside
outside:
        ldaddr r9, x1
        store r9, x1
        store r8, error
        ret
import x1
import error
