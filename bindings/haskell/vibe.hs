-- libvibe — Haskell binding via the FFI. Compile against the shared library:
--
--   ghc vibe.hs -L/path -lvibe -o vibe_hs && LD_LIBRARY_PATH=/path ./vibe_hs
--
-- For the error path we pass a NULL out-error (the C API allows it) and treat
-- a NULL return as "rejected", which keeps the binding free of Storable glue.

{-# LANGUAGE ForeignFunctionInterface #-}

module Main where

import Foreign
import Foreign.C.String
import Foreign.C.Types
import qualified Data.ByteString as BS
import Data.ByteString (useAsCStringLen)
import Data.List (isInfixOf)
import System.Environment (lookupEnv)
import System.Exit (exitFailure, exitSuccess)
import Data.IORef

foreign import ccall "vibe_version"     c_version   :: IO CString
foreign import ccall "vibe_parse"       c_parse     :: CString -> CSize -> Ptr () -> IO (Ptr ())
foreign import ccall "vibe_get_string"  c_getStr    :: Ptr () -> CString -> IO CString
foreign import ccall "vibe_get_int"     c_getInt    :: Ptr () -> CString -> IO CLong
foreign import ccall "vibe_get_float"   c_getFloat  :: Ptr () -> CString -> IO CDouble
foreign import ccall "vibe_get_bool"    c_getBool   :: Ptr () -> CString -> IO CBool
foreign import ccall "vibe_get_array"   c_getArray  :: Ptr () -> CString -> IO (Ptr ())
foreign import ccall "vibe_array_size"  c_arraySize :: Ptr () -> IO CSize
foreign import ccall "vibe_emit"        c_emit      :: Ptr () -> IO CString
foreign import ccall "vibe_free"        c_free      :: Ptr () -> IO ()
foreign import ccall "vibe_value_free"  c_valueFree :: Ptr () -> IO ()

getStr :: Ptr () -> String -> IO String
getStr v path = withCString path $ \cp -> do
  p <- c_getStr v cp
  if p == nullPtr then return "" else peekCString p

getInt :: Ptr () -> String -> IO Integer
getInt v path = withCString path $ \cp -> toInteger <$> c_getInt v cp

getFloat :: Ptr () -> String -> IO Double
getFloat v path = withCString path $ \cp -> realToFrac <$> c_getFloat v cp

getBool :: Ptr () -> String -> IO Bool
getBool v path = withCString path $ \cp -> (/= 0) <$> c_getBool v cp

arraySize :: Ptr () -> String -> IO Int
arraySize v path = withCString path $ \cp -> do
  a <- c_getArray v cp
  if a == nullPtr then return 0 else fromIntegral <$> c_arraySize a

main :: IO ()
main = do
  sample <- maybe "../sample.vibe" id <$> lookupEnv "VIBE_SAMPLE"
  bytes <- BS.readFile sample
  v <- useAsCStringLen bytes $ \(p, n) -> c_parse p (fromIntegral n) nullPtr
  if v == nullPtr
    then putStrLn "FAILED (haskell): parse error" >> exitFailure
    else return ()

  ok <- newIORef True
  let check :: (Show a, Eq a) => String -> a -> a -> IO ()
      check name got want = do
        let pass = got == want
        if pass then return () else writeIORef ok False
        putStrLn $ "  [" ++ (if pass then "ok " else "BAD") ++ "] " ++ name ++ " = " ++ show got

  ver <- c_version >>= peekCString
  check "version" ver "1.2.0"
  getStr v "name"        >>= \x -> check "name" x "libvibe"
  getInt v "answer"      >>= \x -> check "answer" x 42
  pi' <- getFloat v "pi"
  check "pi" (fromIntegral (round (pi' * 100000) :: Integer) / 100000) 3.14159
  getBool v "enabled"    >>= \x -> check "enabled" x True
  getStr v "server.host" >>= \x -> check "server.host" x "localhost"
  getInt v "server.port" >>= \x -> check "server.port" x 8080
  arraySize v "ports"    >>= \x -> check "len(ports)" x 3

  raw <- c_emit v
  emitted <- if raw == nullPtr then return "" else peekCString raw
  if "libvibe" `isInfixOf` emitted
    then putStrLn "  [ok ] emit() round-trips"
    else writeIORef ok False >> putStrLn "  [BAD] emit() did not round-trip"
  if raw /= nullPtr then c_free (castPtr raw) else return ()

  bad <- withCStringLen "name {" $ \(p, n) -> c_parse p (fromIntegral n) nullPtr
  if bad == nullPtr
    then putStrLn "  [ok ] rejects malformed input"
    else writeIORef ok False >> putStrLn "  [BAD] malformed input did not raise"

  c_valueFree v
  final <- readIORef ok
  putStrLn $ if final then "ALL OK (haskell)" else "FAILED (haskell)"
  if final then exitSuccess else exitFailure
